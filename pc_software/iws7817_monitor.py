"""
IWS7817 内部抵抗測定システム - PC コントロールソフト
======================================================
起動: python iws7817_monitor.py

依存ライブラリ:
    pip install matplotlib requests

プロトコル:
  UDP データ受信 (モジュール → PC):
    {"id":"iws7817_01","addr":3,"r":1.23,"v":3.70,"valid":true,"t":12345}

  UDP コマンド送信 (PC → モジュール):
    {"cmd":"start"} / {"cmd":"stop"} / {"cmd":"measure"} /
    {"cmd":"ping"}  / {"cmd":"reboot"}

  HTTP API (PC ↔ モジュール):
    GET  http://<ip>/api/config   → 設定取得
    POST http://<ip>/api/config   → 設定書込 (再起動)
    GET  http://<ip>/api/status   → ステータス取得
    POST http://<ip>/api/start    → 測定開始
    POST http://<ip>/api/stop     → 測定停止
    POST http://<ip>/api/measure  → 1回測定
"""

from __future__ import annotations

import csv
import datetime
import json
import os
import socket
import threading
import time
from collections import deque
from typing import Optional

import tkinter as tk
from tkinter import ttk, messagebox, filedialog, simpledialog

try:
    import requests
except ImportError:
    requests = None  # type: ignore

import matplotlib
matplotlib.use("TkAgg")

# Windows で日本語フォントを設定
from matplotlib import font_manager as _fm
_JP_FONTS = ["Meiryo", "Yu Gothic", "MS Gothic", "MS UI Gothic", "IPAexGothic"]
_available = {f.name for f in _fm.fontManager.ttflist}
for _f in _JP_FONTS:
    if _f in _available:
        matplotlib.rcParams["font.family"] = _f
        break

import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# ── 定数 ──────────────────────────────────────────────────────
MAX_DEVICES     = 10
MAX_HISTORY     = 600
DEFAULT_UDP_PORT = 7817
DEFAULT_CMD_PORT = 7818
HTTP_TIMEOUT    = 5.0
DISCOVER_WAIT   = 2.0   # Ping 待ち時間 (秒)
GRAPH_INTERVAL  = 1000  # グラフ更新間隔 (ms)
SETTINGS_FILE   = os.path.join(os.path.dirname(os.path.abspath(__file__)), "iws7817_settings.json")

DEVICE_COLORS = [
    "#2196F3", "#F44336", "#4CAF50", "#FF9800", "#9C27B0",
    "#00BCD4", "#FF5722", "#8BC34A", "#E91E63", "#795548",
]


# ══════════════════════════════════════════════════════════════
class DeviceInfo:
    """1台の測定モジュールの情報と履歴"""

    def __init__(self, device_id: str, src_ip: str):
        self.device_id   = device_id
        self.ip          = src_ip
        self.i2c_addr    = 0x03
        self.cmd_port    = DEFAULT_CMD_PORT
        self.udp_port    = DEFAULT_UDP_PORT
        self.interval_ms = 2000
        self.measuring   = False
        self.last_r: Optional[float] = None   # mOhm (None = 未受信)
        self.last_v: Optional[float] = None   # V
        self.last_time: Optional[datetime.datetime] = None
        self.r_history: deque[tuple[datetime.datetime, float]] = deque(maxlen=MAX_HISTORY)
        self.v_history: deque[tuple[datetime.datetime, float]] = deque(maxlen=MAX_HISTORY)
        self.visible  = True
        self.color    = "#2196F3"
        self.pkt_count = 0
        self.err_count  = 0


# ══════════════════════════════════════════════════════════════
class UDPReceiver(threading.Thread):
    """UDP パケット受信スレッド (daemon)"""

    def __init__(self, port: int, callback):
        super().__init__(daemon=True, name="UDPReceiver")
        self.port      = port
        self.callback  = callback
        self._stop_ev  = threading.Event()
        self.sock: Optional[socket.socket] = None

    def run(self):
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock.settimeout(1.0)
            self.sock.bind(("", self.port))
        except OSError as e:
            self.callback(None, None, error=str(e))
            return

        while not self._stop_ev.is_set():
            try:
                data, addr = self.sock.recvfrom(1024)
                text = data.decode("utf-8", errors="ignore")
                self.callback(text, addr[0])
            except socket.timeout:
                pass
            except OSError:
                break

        try:
            self.sock.close()
        except Exception:
            pass

    def stop(self):
        self._stop_ev.set()


# ══════════════════════════════════════════════════════════════
class ConfigDialog(tk.Toplevel):
    """HTTP 経由でデバイス設定を読み書きするダイアログ"""

    FIELDS = [
        ("デバイスID",           "device_id",   "str"),
        ("WiFi SSID",            "wifi_ssid",   "str"),
        ("WiFi パスワード",       "wifi_pass",   "pw"),
        ("送信先 IP アドレス",   "target_ip",   "str"),
        ("UDP データポート",      "udp_port",    "int"),
        ("UDP コマンドポート",    "cmd_port",    "int"),
        ("I2C アドレス (hex)",   "i2c_addr",    "hex"),
        ("測定間隔 (ms, ≥1000)", "interval_ms", "int"),
    ]

    def __init__(self, parent: tk.Widget, device: DeviceInfo):
        super().__init__(parent)
        self.device = device
        self.title(f"設定: {device.device_id}  [{device.ip}]")
        self.resizable(False, False)
        self.grab_set()
        self._build()
        self._load()

    def _build(self):
        pad = dict(padx=8, pady=3, sticky="ew")
        self.columnconfigure(1, weight=1)
        self.vars: dict[str, tk.StringVar] = {}
        for row, (label, key, kind) in enumerate(self.FIELDS):
            tk.Label(self, text=label, anchor="e", width=20).grid(row=row, column=0, **pad)
            var = tk.StringVar()
            show = "*" if kind == "pw" else ""
            ent = tk.Entry(self, textvariable=var, width=26, show=show)
            ent.grid(row=row, column=1, **pad)
            self.vars[key] = var

        n = len(self.FIELDS)
        self.auto_var = tk.BooleanVar()
        tk.Checkbutton(self, text="起動時に測定を自動開始", variable=self.auto_var).grid(
            row=n, column=0, columnspan=2, pady=4)

        btn = tk.Frame(self)
        btn.grid(row=n + 1, column=0, columnspan=2, pady=10)
        tk.Button(btn, text="読込",       command=self._load,  width=8).pack(side="left", padx=4)
        tk.Button(btn, text="書込/再起動", command=self._save,
                  bg="#1565C0", fg="white", width=12).pack(side="left", padx=4)
        tk.Button(btn, text="閉じる",     command=self.destroy, width=8).pack(side="left", padx=4)

    def _load(self):
        if requests is None:
            messagebox.showerror("エラー", "requests ライブラリが必要です\n  pip install requests", parent=self)
            return
        try:
            r = requests.get(f"http://{self.device.ip}/api/config", timeout=HTTP_TIMEOUT)
            r.raise_for_status()
            cfg = r.json()
        except Exception as e:
            messagebox.showerror("読込エラー", str(e), parent=self)
            return

        for label, key, kind in self.FIELDS:
            if key == "wifi_pass":
                self.vars[key].set("")   # パスワードは表示しない
            elif key == "i2c_addr":
                self.vars[key].set(f"0x{cfg.get(key, 3):02X}")
            else:
                self.vars[key].set(str(cfg.get(key, "")))
        self.auto_var.set(cfg.get("auto_measure", True))

    def _save(self):
        if requests is None:
            messagebox.showerror("エラー", "requests ライブラリが必要です\n  pip install requests", parent=self)
            return
        data: dict = {}
        for label, key, kind in self.FIELDS:
            val = self.vars[key].get().strip()
            if key == "wifi_pass" and not val:
                continue   # 空欄はスキップ (変更なし)
            try:
                if kind == "int":
                    data[key] = int(val)
                elif kind == "hex":
                    data[key] = int(val, 16) if val.lower().startswith("0x") else int(val)
                else:
                    data[key] = val
            except ValueError as e:
                messagebox.showerror("入力エラー", f"{label}: {e}", parent=self)
                return

        if "interval_ms" in data and data["interval_ms"] < 1000:
            messagebox.showerror("入力エラー", "測定間隔は1000ms以上にしてください", parent=self)
            return

        data["auto_measure"] = self.auto_var.get()

        try:
            r = requests.post(
                f"http://{self.device.ip}/api/config",
                json=data,
                timeout=HTTP_TIMEOUT,
            )
            resp = r.json()
            if resp.get("ok"):
                messagebox.showinfo("完了", "設定を保存しました。\nデバイスが再起動します。", parent=self)
                self.destroy()
            else:
                messagebox.showerror("エラー", f"保存失敗:\n{resp}", parent=self)
        except Exception as e:
            messagebox.showerror("書込エラー", str(e), parent=self)


# ══════════════════════════════════════════════════════════════
class App(tk.Tk):
    """メインアプリケーション"""

    def __init__(self):
        super().__init__()
        self.title("IWS7817 内部抵抗測定システム")
        self.geometry("1280x760")
        self.minsize(900, 600)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

        self._lock    = threading.Lock()
        self.devices: dict[str, DeviceInfo] = {}
        self._dev_widgets: dict[str, dict] = {}
        self.receiver: Optional[UDPReceiver] = None
        self._log_fp   = None
        self._log_csv  = None

        self.udp_port_var   = tk.IntVar(value=DEFAULT_UDP_PORT)
        self.cmd_port_var   = tk.IntVar(value=DEFAULT_CMD_PORT)
        self.listening      = False
        self.graph_type_var = tk.StringVar(value="resistance")
        self.status_var     = tk.StringVar(value="受信停止中")

        self._build_menu()
        self._build_ui()
        self._schedule_graph_update()
        self._load_settings()

    # ── メニュー ───────────────────────────────────────────────
    def _build_menu(self):
        mb = tk.Menu(self)
        self.config(menu=mb)

        fm = tk.Menu(mb, tearoff=0)
        mb.add_cascade(label="ファイル", menu=fm)
        fm.add_command(label="CSVログ開始...", command=self._start_csv_log)
        fm.add_command(label="CSVログ停止",   command=self._stop_csv_log)
        fm.add_separator()
        fm.add_command(label="接続先設定を保存...",   command=self._save_settings_dialog)
        fm.add_command(label="接続先設定を読込...",   command=self._load_settings_dialog)
        fm.add_separator()
        fm.add_command(label="終了",           command=self._on_close)

        dm = tk.Menu(mb, tearoff=0)
        mb.add_cascade(label="デバイス", menu=dm)
        dm.add_command(label="デバイス探索 (broadcast ping)", command=self._discover)
        dm.add_command(label="IPアドレスで追加...",           command=self._add_by_ip)
        dm.add_separator()
        dm.add_command(label="全デバイスに測定開始",          command=self._cmd_all_start)
        dm.add_command(label="全デバイスに測定停止",          command=self._cmd_all_stop)

    # ── UI レイアウト ──────────────────────────────────────────
    def _build_ui(self):
        # ── 上部: 受信制御バー
        top_bar = tk.Frame(self, relief="ridge", bd=1, pady=4)
        top_bar.pack(fill="x", padx=4, pady=(4, 0))

        tk.Label(top_bar, text="UDP受信ポート:").pack(side="left", padx=(8, 2))
        tk.Entry(top_bar, textvariable=self.udp_port_var, width=7).pack(side="left")
        self._listen_btn = tk.Button(
            top_bar, text="受信開始", width=10, command=self._toggle_listen,
            bg="#1565C0", fg="white")
        self._listen_btn.pack(side="left", padx=6)
        tk.Label(top_bar, textvariable=self.status_var, fg="gray").pack(side="left", padx=4)

        tk.Label(top_bar, text="コマンドポート:").pack(side="left", padx=(16, 2))
        tk.Entry(top_bar, textvariable=self.cmd_port_var, width=7).pack(side="left")

        tk.Button(top_bar, text="全開始 ▶", bg="#2E7D32", fg="white",
                  command=self._cmd_all_start).pack(side="right", padx=4)
        tk.Button(top_bar, text="全停止 ■", bg="#C62828", fg="white",
                  command=self._cmd_all_stop).pack(side="right", padx=4)
        tk.Button(top_bar, text="デバイス探索", command=self._discover).pack(side="right", padx=4)

        # ── 中段: 左=デバイスリスト、右=グラフ
        main = tk.PanedWindow(self, orient="horizontal", sashwidth=6)
        main.pack(fill="both", expand=True, padx=4, pady=4)

        # 左パネル
        left = tk.Frame(main, width=240, bg="#F5F5F5")
        main.add(left, minsize=200)

        tk.Label(left, text="接続デバイス", bg="#F5F5F5",
                 font=("", 10, "bold")).pack(pady=(8, 2))

        self._dev_scroll_canvas = tk.Canvas(left, bg="#F5F5F5", highlightthickness=0)
        dev_vsb = tk.Scrollbar(left, orient="vertical", command=self._dev_scroll_canvas.yview)
        self._dev_scroll_canvas.configure(yscrollcommand=dev_vsb.set)
        dev_vsb.pack(side="right", fill="y")
        self._dev_scroll_canvas.pack(fill="both", expand=True)
        self._dev_list_frame = tk.Frame(self._dev_scroll_canvas, bg="#F5F5F5")
        self._dev_scroll_canvas.create_window((0, 0), window=self._dev_list_frame, anchor="nw")
        self._dev_list_frame.bind(
            "<Configure>",
            lambda e: self._dev_scroll_canvas.configure(
                scrollregion=self._dev_scroll_canvas.bbox("all")))

        # 右パネル (グラフ)
        right = tk.Frame(main)
        main.add(right, minsize=500)

        graph_ctrl = tk.Frame(right)
        graph_ctrl.pack(fill="x", pady=2)
        tk.Label(graph_ctrl, text="表示:").pack(side="left")
        tk.Radiobutton(graph_ctrl, text="内部抵抗 (mΩ)", variable=self.graph_type_var,
                       value="resistance", command=self._redraw_graph).pack(side="left")
        tk.Radiobutton(graph_ctrl, text="電圧 (V)", variable=self.graph_type_var,
                       value="voltage", command=self._redraw_graph).pack(side="left")
        tk.Button(graph_ctrl, text="グラフクリア", command=self._clear_graph).pack(side="right", padx=6)

        self.fig, self.ax = plt.subplots(figsize=(9, 5))
        self.fig.patch.set_facecolor("#FAFAFA")
        self.ax.set_facecolor("#F0F0F0")
        self.fig.tight_layout(pad=2)
        self.canvas = FigureCanvasTkAgg(self.fig, master=right)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

        # ── 下部: ログエリア
        log_frame = tk.Frame(self, relief="sunken", bd=1)
        log_frame.pack(fill="x", padx=4, pady=(0, 4))
        self.log_text = tk.Text(
            log_frame, height=5, state="disabled",
            font=("Consolas", 9), bg="#1E1E1E", fg="#D4D4D4",
            insertbackground="white")
        sb = tk.Scrollbar(log_frame, command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self.log_text.pack(fill="x")

    # ── ログ出力 ───────────────────────────────────────────────
    def _log(self, msg: str, color: str = "#D4D4D4"):
        ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
        line = f"[{ts}] {msg}\n"
        self.log_text.config(state="normal")
        self.log_text.insert("end", line)
        self.log_text.see("end")
        # 行数が多すぎる場合は古い行を削除
        line_count = int(self.log_text.index("end-1c").split(".")[0])
        if line_count > 500:
            self.log_text.delete("1.0", "100.0")
        self.log_text.config(state="disabled")

    # ── UDP 受信 ───────────────────────────────────────────────
    def _toggle_listen(self):
        if self.listening:
            self._stop_listen()
        else:
            self._start_listen()

    def _start_listen(self):
        port = self.udp_port_var.get()

        def _on_packet(text, src_ip, error=None):
            if error:
                self.after(0, lambda: (
                    self._log(f"UDP受信エラー: {error}"),
                    messagebox.showerror("UDP エラー", f"ポート {port} を開けません:\n{error}")
                ))
                return
            if text and src_ip:
                self.after(0, lambda t=text, ip=src_ip: self._on_udp_packet(t, ip))

        self.receiver = UDPReceiver(port, _on_packet)
        self.receiver.start()
        self.listening = True
        self._listen_btn.config(text="受信停止", bg="#C62828")
        self.status_var.set(f"受信中 :{port}")
        self._log(f"UDP受信開始 ポート:{port}")

    def _stop_listen(self):
        if self.receiver:
            self.receiver.stop()
            self.receiver = None
        self.listening = False
        self._listen_btn.config(text="受信開始", bg="#1565C0")
        self.status_var.set("受信停止中")
        self._log("UDP受信停止")

    # ── UDP パケット処理 ───────────────────────────────────────
    def _on_udp_packet(self, text: str, src_ip: str):
        try:
            pkt = json.loads(text)
        except json.JSONDecodeError:
            return

        dev_id = pkt.get("id", src_ip)
        r_val  = float(pkt.get("r", -999.0))
        v_val  = float(pkt.get("v", -999.0))
        valid  = bool(pkt.get("valid", False))
        now    = datetime.datetime.now()

        is_new = False
        with self._lock:
            if dev_id not in self.devices:
                if len(self.devices) >= MAX_DEVICES:
                    self._log(f"警告: デバイス上限({MAX_DEVICES}台)に達しています。{dev_id}は無視します。")
                    return
                dev = DeviceInfo(dev_id, src_ip)
                dev.color = DEVICE_COLORS[len(self.devices) % len(DEVICE_COLORS)]
                self.devices[dev_id] = dev
                is_new = True

            dev = self.devices[dev_id]
            dev.ip        = src_ip
            dev.measuring = True
            dev.pkt_count += 1

            if valid and r_val != -999.0 and v_val != -999.0:
                dev.last_r    = r_val
                dev.last_v    = v_val
                dev.last_time = now
                # 特殊値 (-1.0=範囲外, 99.0=上限超) はグラフから除外
                if r_val not in (-1.0, -999.0):
                    dev.r_history.append((now, r_val))
                if v_val not in (-1.0, 99.0, -999.0):
                    dev.v_history.append((now, v_val))
            else:
                dev.err_count += 1

        if is_new:
            self._log(f"新デバイス検出: {dev_id} ({src_ip})")
            self.after(0, lambda d=dev_id: self._add_device_widget(d))

        self._write_csv_row(now, dev_id, src_ip, r_val, v_val, valid)
        self.after(0, lambda d=dev_id: self._update_device_widget(d))

    # ── デバイスウィジェット ───────────────────────────────────
    def _add_device_widget(self, dev_id: str):
        dev = self.devices.get(dev_id)
        if dev is None or dev_id in self._dev_widgets:
            return

        row = tk.Frame(self._dev_list_frame, relief="groove", bd=1, bg="white", pady=2)
        row.pack(fill="x", pady=2, padx=2)

        tk.Label(row, bg=dev.color, width=3).pack(side="left", fill="y")

        info = tk.Frame(row, bg="white")
        info.pack(side="left", fill="x", expand=True, padx=4)
        tk.Label(info, text=dev_id, bg="white", font=("", 9, "bold"), anchor="w").pack(fill="x")
        ip_lbl = tk.Label(info, text=dev.ip, bg="white", fg="#777", font=("", 8), anchor="w")
        ip_lbl.pack(fill="x")
        val_lbl = tk.Label(info, text="R:--- V:---", bg="white",
                           font=("Consolas", 8), anchor="w")
        val_lbl.pack(fill="x")
        cnt_lbl = tk.Label(info, text="pkt:0 err:0", bg="white", fg="#999",
                           font=("", 7), anchor="w")
        cnt_lbl.pack(fill="x")

        ctrl = tk.Frame(row, bg="white")
        ctrl.pack(side="right", padx=2)
        tk.Button(ctrl, text="▶", width=2, bg="#2E7D32", fg="white", font=("", 9),
                  command=lambda d=dev_id: self._cmd_start(d)).pack(pady=1)
        tk.Button(ctrl, text="■", width=2, bg="#C62828", fg="white", font=("", 9),
                  command=lambda d=dev_id: self._cmd_stop(d)).pack(pady=1)
        tk.Button(ctrl, text="⚙", width=2, font=("", 9),
                  command=lambda d=dev_id: self._open_config(d)).pack(pady=1)

        vis_var = tk.BooleanVar(value=True)
        tk.Checkbutton(ctrl, variable=vis_var, bg="white",
                       command=lambda d=dev_id, v=vis_var: self._toggle_vis(d, v)).pack()

        self._dev_widgets[dev_id] = {
            "row": row, "val_lbl": val_lbl, "cnt_lbl": cnt_lbl,
            "ip_lbl": ip_lbl, "vis_var": vis_var,
        }

    def _update_device_widget(self, dev_id: str):
        w = self._dev_widgets.get(dev_id)
        if not w:
            return
        dev = self.devices.get(dev_id)
        if not dev:
            return

        r_str = f"R:{dev.last_r:8.3f}mΩ" if dev.last_r is not None else "R:    ---  "
        v_str = f"V:{dev.last_v:7.4f}V"   if dev.last_v is not None else "V:    ---"
        w["val_lbl"].config(text=f"{r_str}  {v_str}")
        w["ip_lbl"].config(text=dev.ip)
        w["cnt_lbl"].config(text=f"pkt:{dev.pkt_count}  err:{dev.err_count}")

    def _toggle_vis(self, dev_id: str, var: tk.BooleanVar):
        with self._lock:
            if dev_id in self.devices:
                self.devices[dev_id].visible = var.get()

    # ── UDP コマンド送信 ───────────────────────────────────────
    def _send_cmd(self, dev: DeviceInfo, cmd: dict):
        def _do():
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                s.settimeout(2.0)
                s.sendto(json.dumps(cmd).encode(), (dev.ip, dev.cmd_port))
                s.close()
            except Exception as e:
                self.after(0, lambda: self._log(f"CMD失敗 {dev.device_id}: {e}"))
        threading.Thread(target=_do, daemon=True).start()

    def _cmd_start(self, dev_id: str):
        dev = self.devices.get(dev_id)
        if dev:
            self._send_cmd(dev, {"cmd": "start"})
            self._log(f"[CMD] START → {dev_id} ({dev.ip})")

    def _cmd_stop(self, dev_id: str):
        dev = self.devices.get(dev_id)
        if dev:
            self._send_cmd(dev, {"cmd": "stop"})
            self._log(f"[CMD] STOP → {dev_id} ({dev.ip})")

    def _cmd_all_start(self):
        for dev_id in list(self.devices.keys()):
            self._cmd_start(dev_id)

    def _cmd_all_stop(self):
        for dev_id in list(self.devices.keys()):
            self._cmd_stop(dev_id)

    def _open_config(self, dev_id: str):
        dev = self.devices.get(dev_id)
        if dev:
            if requests is None:
                messagebox.showerror("エラー", "requests ライブラリが必要です\n  pip install requests")
                return
            ConfigDialog(self, dev)

    # ── デバイス探索 (broadcast ping) ─────────────────────────
    def _discover(self):
        cmd_port = self.cmd_port_var.get()
        listen_port = self.udp_port_var.get()

        def _do():
            responses = []
            try:
                # 送受信用ソケット
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                s.settimeout(DISCOVER_WAIT)
                s.bind(("", 0))  # 任意ポートでバインド
                self.after(0, lambda: self._log(f"探索中... (broadcast:{cmd_port}, 待ち:{DISCOVER_WAIT}s)"))
                ping = json.dumps({"cmd": "ping"}).encode()
                s.sendto(ping, ("255.255.255.255", cmd_port))

                deadline = time.time() + DISCOVER_WAIT
                while time.time() < deadline:
                    try:
                        data, addr = s.recvfrom(512)
                        pkt = json.loads(data.decode())
                        if pkt.get("pong"):
                            responses.append((pkt, addr[0]))
                    except socket.timeout:
                        break
                    except Exception:
                        pass
                s.close()
            except Exception as e:
                self.after(0, lambda: self._log(f"探索エラー: {e}"))
                return

            def _process():
                if not responses:
                    self._log("デバイスが見つかりませんでした。")
                    return
                for pkt, src_ip in responses:
                    dev_id = pkt.get("id", src_ip)
                    self._log(f"応答: {dev_id} ({src_ip})  measuring={pkt.get('measuring')}")
                    # 未登録なら自動追加
                    if dev_id not in self.devices:
                        dev = DeviceInfo(dev_id, src_ip)
                        dev.color    = DEVICE_COLORS[len(self.devices) % len(DEVICE_COLORS)]
                        dev.cmd_port = pkt.get("cmd_port", DEFAULT_CMD_PORT)
                        dev.udp_port = pkt.get("udp_port", DEFAULT_UDP_PORT)
                        with self._lock:
                            self.devices[dev_id] = dev
                        self._add_device_widget(dev_id)
            self.after(0, _process)

        threading.Thread(target=_do, daemon=True).start()

    def _add_by_ip(self):
        ip = simpledialog.askstring("IPアドレスで追加", "デバイスの IP アドレスを入力:", parent=self)
        if not ip:
            return
        ip = ip.strip()

        def _do():
            try:
                r = requests.get(f"http://{ip}/api/status", timeout=HTTP_TIMEOUT)
                info = r.json()
                dev_id = info.get("device_id", ip)
                dev = DeviceInfo(dev_id, ip)
                dev.color    = DEVICE_COLORS[len(self.devices) % len(DEVICE_COLORS)]
                dev.i2c_addr = info.get("i2c_addr", 0x03)
                with self._lock:
                    if dev_id not in self.devices:
                        self.devices[dev_id] = dev
                self.after(0, lambda d=dev_id: (
                    self._add_device_widget(d),
                    self._log(f"追加: {d} ({ip})")
                ))
            except Exception as e:
                self.after(0, lambda: messagebox.showerror("接続エラー", f"{ip}\n{e}"))
        if requests is None:
            messagebox.showerror("エラー", "requests ライブラリが必要です\n  pip install requests")
            return
        threading.Thread(target=_do, daemon=True).start()

    # ── グラフ ─────────────────────────────────────────────────
    def _schedule_graph_update(self):
        self._redraw_graph()
        self.after(GRAPH_INTERVAL, self._schedule_graph_update)

    def _redraw_graph(self):
        self.ax.clear()
        gtype = self.graph_type_var.get()

        if gtype == "resistance":
            self.ax.set_ylabel("内部抵抗 (mΩ)", fontsize=10)
            self.ax.set_title("蓄電池セル 内部抵抗", fontsize=11)
        else:
            self.ax.set_ylabel("電圧 (V)", fontsize=10)
            self.ax.set_title("蓄電池セル 電圧", fontsize=11)

        self.ax.set_xlabel("時刻", fontsize=9)
        self.ax.grid(True, alpha=0.35, linestyle="--")
        self.ax.set_facecolor("#F8F8F8")

        with self._lock:
            snap = list(self.devices.values())

        has_data = False
        for dev in snap:
            if not dev.visible:
                continue
            hist = dev.r_history if gtype == "resistance" else dev.v_history
            if len(hist) < 1:
                continue
            times  = [h[0] for h in hist]
            values = [h[1] for h in hist]
            self.ax.plot(
                times, values,
                color=dev.color, label=dev.device_id,
                linewidth=1.8, marker="o", markersize=2.5, markevery=5,
            )
            # 最新値をラベル表示
            if values:
                self.ax.annotate(
                    f"{values[-1]:.2f}",
                    xy=(times[-1], values[-1]),
                    fontsize=7, color=dev.color,
                    xytext=(4, 0), textcoords="offset points",
                )
            has_data = True

        if has_data:
            self.ax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M:%S"))
            self.fig.autofmt_xdate(rotation=25, ha="right")
            self.ax.legend(loc="upper left", fontsize=8, framealpha=0.8)

        self.fig.tight_layout(pad=1.5)
        self.canvas.draw_idle()

    def _clear_graph(self):
        with self._lock:
            for dev in self.devices.values():
                dev.r_history.clear()
                dev.v_history.clear()
        self._log("グラフデータをクリアしました。")

    # ── CSV ログ ───────────────────────────────────────────────
    def _start_csv_log(self):
        default_name = f"iws7817_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        fpath = filedialog.asksaveasfilename(
            initialfile=default_name,
            defaultextension=".csv",
            filetypes=[("CSV ファイル", "*.csv"), ("全ファイル", "*.*")],
            title="CSV ログ保存先",
        )
        if not fpath:
            return
        try:
            self._log_fp  = open(fpath, "w", newline="", encoding="utf-8-sig")
            self._log_csv = csv.writer(self._log_fp)
            self._log_csv.writerow(
                ["timestamp", "device_id", "src_ip",
                 "resistance_mOhm", "voltage_V", "valid"])
            self._log(f"CSVログ開始: {fpath}")
        except Exception as e:
            messagebox.showerror("エラー", f"ファイルを開けません:\n{e}")

    def _stop_csv_log(self):
        if self._log_fp:
            self._log_fp.close()
            self._log_fp  = None
            self._log_csv = None
            self._log("CSVログ停止")

    def _write_csv_row(self, ts: datetime.datetime, dev_id: str, ip: str,
                       r: float, v: float, valid: bool):
        if self._log_csv is None:
            return
        try:
            self._log_csv.writerow([
                ts.isoformat(), dev_id, ip,
                f"{r:.4f}" if valid else "",
                f"{v:.4f}" if valid else "",
                "1" if valid else "0",
            ])
            self._log_fp.flush()
        except Exception:
            pass

    # ── 接続先設定の保存/読込 ──────────────────────────────────
    def _save_settings(self, path: str = SETTINGS_FILE):
        data = {
            "udp_port": self.udp_port_var.get(),
            "cmd_port": self.cmd_port_var.get(),
            "devices": [
                {
                    "device_id":   dev.device_id,
                    "ip":          dev.ip,
                    "i2c_addr":    dev.i2c_addr,
                    "cmd_port":    dev.cmd_port,
                    "udp_port":    dev.udp_port,
                    "interval_ms": dev.interval_ms,
                    "color":       dev.color,
                    "visible":     dev.visible,
                }
                for dev in self.devices.values()
            ],
        }
        try:
            with open(path, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            self._log(f"設定を保存しました: {path}")
        except Exception as e:
            messagebox.showerror("エラー", f"設定ファイルの保存に失敗しました:\n{e}")

    def _load_settings(self, path: str = SETTINGS_FILE):
        if not os.path.exists(path):
            return
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except Exception as e:
            self._log(f"設定ファイルの読込に失敗しました: {e}")
            return

        self.udp_port_var.set(data.get("udp_port", DEFAULT_UDP_PORT))
        self.cmd_port_var.set(data.get("cmd_port", DEFAULT_CMD_PORT))

        for dev_data in data.get("devices", []):
            dev_id = dev_data.get("device_id")
            ip     = dev_data.get("ip")
            if not dev_id or not ip:
                continue
            if dev_id in self.devices:
                continue
            if len(self.devices) >= MAX_DEVICES:
                break
            dev             = DeviceInfo(dev_id, ip)
            dev.i2c_addr    = dev_data.get("i2c_addr",    0x03)
            dev.cmd_port    = dev_data.get("cmd_port",    DEFAULT_CMD_PORT)
            dev.udp_port    = dev_data.get("udp_port",    DEFAULT_UDP_PORT)
            dev.interval_ms = dev_data.get("interval_ms", 2000)
            dev.color       = dev_data.get("color", DEVICE_COLORS[len(self.devices) % len(DEVICE_COLORS)])
            dev.visible     = dev_data.get("visible",     True)
            with self._lock:
                self.devices[dev_id] = dev
            self._add_device_widget(dev_id)

        self._log(f"設定を読み込みました: {path}  ({len(data.get('devices', []))} 台)")

    def _save_settings_dialog(self):
        path = filedialog.asksaveasfilename(
            initialfile=os.path.basename(SETTINGS_FILE),
            initialdir=os.path.dirname(SETTINGS_FILE),
            defaultextension=".json",
            filetypes=[("JSON ファイル", "*.json"), ("全ファイル", "*.*")],
            title="接続先設定の保存",
        )
        if path:
            self._save_settings(path)

    def _load_settings_dialog(self):
        path = filedialog.askopenfilename(
            initialdir=os.path.dirname(SETTINGS_FILE),
            defaultextension=".json",
            filetypes=[("JSON ファイル", "*.json"), ("全ファイル", "*.*")],
            title="接続先設定の読込",
        )
        if path:
            self._load_settings(path)

    # ── 終了 ───────────────────────────────────────────────────
    def _on_close(self):
        self._stop_listen()
        self._stop_csv_log()
        self._save_settings()
        plt.close("all")
        self.destroy()


# ── エントリポイント ───────────────────────────────────────────
if __name__ == "__main__":
    app = App()
    app.mainloop()

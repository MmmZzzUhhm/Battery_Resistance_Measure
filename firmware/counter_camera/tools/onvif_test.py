#!/usr/bin/env python3
"""
薄型カウンタカメラ ONVIF簡易テストツール

実機(firmware/counter_camera)のONVIFエンドポイントに対して一通りのSOAPリクエストを送り、
応答内容を検証する。ハードウェア動作確認(parent_hwtest等)と同じ「PASS/FAIL一覧」形式で
結果を表示する。

使い方:
    python onvif_test.py --host 192.168.4.1
    python onvif_test.py --host 192.168.4.1 --save-dir ./snapshots

依存: requests (pip install requests)。XML解析はPython標準のxml.etree.ElementTreeを使用。
"""
import argparse
import sys
import time
import xml.etree.ElementTree as ET

import requests

RESULTS = []  # (name, ok, detail)


def report(name, ok, detail=""):
    RESULTS.append((name, ok, detail))
    mark = "OK" if ok else "FAIL"
    print(f"[{mark:4}] {name:28} {detail}")


def local_name(tag):
    return tag.split("}")[-1] if "}" in tag else tag


def find_text(root, name):
    for el in root.iter():
        if local_name(el.tag) == name:
            return el.text
    return None


def soap_envelope(inner_xml):
    return (
        '<?xml version="1.0" encoding="UTF-8"?>'
        '<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope">'
        "<soap:Body>" + inner_xml + "</soap:Body></soap:Envelope>"
    )


def soap_post(base_url, path, inner_xml, timeout=5):
    url = base_url + path
    body = soap_envelope(inner_xml)
    resp = requests.post(
        url, data=body.encode("utf-8"),
        headers={"Content-Type": "application/soap+xml"}, timeout=timeout,
    )
    return resp


# ---------------------------------------------------------------- device_service

def test_device_service(base_url):
    actions = {
        "GetDeviceInformation": '<tds:GetDeviceInformation xmlns:tds="http://www.onvif.org/ver10/device/wsdl"/>',
        "GetCapabilities": '<tds:GetCapabilities xmlns:tds="http://www.onvif.org/ver10/device/wsdl"><tds:Category>All</tds:Category></tds:GetCapabilities>',
        "GetServices": '<tds:GetServices xmlns:tds="http://www.onvif.org/ver10/device/wsdl"><tds:IncludeCapability>true</tds:IncludeCapability></tds:GetServices>',
        "GetSystemDateAndTime": '<tds:GetSystemDateAndTime xmlns:tds="http://www.onvif.org/ver10/device/wsdl"/>',
    }
    for name, xml_body in actions.items():
        try:
            resp = soap_post(base_url, "/onvif/device_service", xml_body)
            if resp.status_code != 200:
                report(f"device:{name}", False, f"HTTP {resp.status_code}")
                continue
            root = ET.fromstring(resp.text)
            if name == "GetDeviceInformation":
                detail = f"Model={find_text(root, 'Model')} Serial={find_text(root, 'SerialNumber')}"
            elif name == "GetCapabilities":
                detail = f"Media XAddr={find_text(root, 'Media') and find_text(root, 'XAddr')}"
            elif name == "GetServices":
                ns_count = sum(1 for el in root.iter() if local_name(el.tag) == "Service")
                detail = f"{ns_count} service(s)"
            else:
                detail = f"{find_text(root, 'Year')}-{find_text(root, 'Month')}-{find_text(root, 'Day')} {find_text(root, 'Hour')}:{find_text(root, 'Minute')}:{find_text(root, 'Second')}"
            report(f"device:{name}", True, detail)
        except Exception as e:
            report(f"device:{name}", False, str(e))


# ----------------------------------------------------------------- media_service

def test_media_service(base_url):
    snapshot_uri = None

    try:
        resp = soap_post(base_url, "/onvif/media_service",
                          '<trt:GetProfiles xmlns:trt="http://www.onvif.org/ver10/media/wsdl"/>')
        root = ET.fromstring(resp.text)
        w, h = find_text(root, "Width"), find_text(root, "Height")
        report("media:GetProfiles", resp.status_code == 200, f"{w}x{h}")
    except Exception as e:
        report("media:GetProfiles", False, str(e))

    try:
        resp = soap_post(base_url, "/onvif/media_service",
                          '<trt:GetVideoSources xmlns:trt="http://www.onvif.org/ver10/media/wsdl"/>')
        report("media:GetVideoSources", resp.status_code == 200, "")
    except Exception as e:
        report("media:GetVideoSources", False, str(e))

    try:
        resp = soap_post(base_url, "/onvif/media_service",
                          '<trt:GetSnapshotUri xmlns:trt="http://www.onvif.org/ver10/media/wsdl">'
                          '<trt:ProfileToken>Profile_1</trt:ProfileToken></trt:GetSnapshotUri>')
        root = ET.fromstring(resp.text)
        snapshot_uri = find_text(root, "Uri")
        report("media:GetSnapshotUri", bool(snapshot_uri), snapshot_uri or "")
    except Exception as e:
        report("media:GetSnapshotUri", False, str(e))

    return snapshot_uri


def test_snapshot(uri, save_dir):
    if not uri:
        report("snapshot:fetch", False, "GetSnapshotUriが失敗したためスキップ")
        return
    try:
        resp = requests.get(uri, timeout=10)
        body = resp.content
        is_jpeg = len(body) >= 2 and body[0:2] == b"\xff\xd8"
        detail = f"{len(body)} bytes, JPEG SOI={'OK' if is_jpeg else 'NG'}"
        report("snapshot:fetch", resp.status_code == 200 and is_jpeg, detail)
        if save_dir and is_jpeg:
            path = f"{save_dir}/onvif_test_{int(time.time())}.jpg"
            with open(path, "wb") as f:
                f.write(body)
            print(f"       -> saved to {path}")
    except Exception as e:
        report("snapshot:fetch", False, str(e))


# --------------------------------------------------------------- imaging_service

def test_imaging_service(base_url):
    try:
        resp = soap_post(base_url, "/onvif/imaging_service",
                          '<timg:GetImagingSettings xmlns:timg="http://www.onvif.org/ver20/imaging/wsdl">'
                          '<timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken></timg:GetImagingSettings>')
        root = ET.fromstring(resp.text)
        before = find_text(root, "Brightness")
        report("imaging:GetImagingSettings", resp.status_code == 200, f"Brightness={before}")
    except Exception as e:
        report("imaging:GetImagingSettings", False, str(e))
        before = None

    try:
        target = "75" if before != "75" else "50"
        resp = soap_post(base_url, "/onvif/imaging_service",
                          '<timg:SetImagingSettings xmlns:timg="http://www.onvif.org/ver20/imaging/wsdl" '
                          'xmlns:tt="http://www.onvif.org/ver10/schema">'
                          '<timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>'
                          f'<timg:ImagingSettings><tt:Brightness>{target}</tt:Brightness></timg:ImagingSettings>'
                          "</timg:SetImagingSettings>")
        report("imaging:SetImagingSettings", resp.status_code == 200, f"Brightness -> {target}")
    except Exception as e:
        report("imaging:SetImagingSettings", False, str(e))

    try:
        resp = soap_post(base_url, "/onvif/imaging_service",
                          '<timg:GetOptions xmlns:timg="http://www.onvif.org/ver20/imaging/wsdl">'
                          '<timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken></timg:GetOptions>')
        report("imaging:GetOptions", resp.status_code == 200, "")
    except Exception as e:
        report("imaging:GetOptions", False, str(e))


# ------------------------------------------------------------------- light (独自)

def test_light(base_url):
    for state in ("on", "off"):
        try:
            resp = requests.get(f"{base_url}/onvif/light/{state}", timeout=5)
            ok = resp.status_code == 200 and resp.json().get("ok") is True
            report(f"light:{state}", ok, resp.text)
        except Exception as e:
            report(f"light:{state}", False, str(e))
    try:
        resp = requests.get(f"{base_url}/onvif/light/status", timeout=5)
        report("light:status", resp.status_code == 200, resp.text)
    except Exception as e:
        report("light:status", False, str(e))


# --------------------------------------------------------------------- local API

def test_local_api(base_url):
    try:
        resp = requests.get(f"{base_url}/api/status", timeout=5)
        report("local:/api/status", resp.status_code == 200, resp.text)
    except Exception as e:
        report("local:/api/status", False, str(e))


def print_summary():
    print("\n==================== 結果サマリ ====================")
    ok_count = 0
    for name, ok, detail in RESULTS:
        print(f"  [{'OK' if ok else 'FAIL'}] {name}")
        ok_count += 1 if ok else 0
    print("-----------------------------------------------------")
    print(f"  {ok_count} / {len(RESULTS)} 項目 PASS")
    print("=====================================================")


def main():
    ap = argparse.ArgumentParser(description="薄型カウンタカメラ ONVIF簡易テストツール")
    ap.add_argument("--host", default="192.168.4.1", help="デバイスのIPアドレス (既定: 192.168.4.1)")
    ap.add_argument("--save-dir", default=None, help="取得したスナップショットJPEGの保存先ディレクトリ")
    ap.add_argument("--skip-light", action="store_true", help="照明制御テストをスキップする")
    args = ap.parse_args()

    base_url = f"http://{args.host}"
    print(f"対象デバイス: {base_url}\n")

    test_local_api(base_url)
    test_device_service(base_url)
    snapshot_uri = test_media_service(base_url)
    test_snapshot(snapshot_uri, args.save_dir)
    test_imaging_service(base_url)
    if not args.skip_light:
        test_light(base_url)

    print_summary()
    sys.exit(0 if all(ok for _, ok, _ in RESULTS) else 1)


if __name__ == "__main__":
    main()

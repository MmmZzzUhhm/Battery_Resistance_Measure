// VCBカウンター認識(別チーム開発)への連携。
// tools/ocr_bridge.py 経由で app.camera_interface.recognize_counter() を呼び出す。
// OpenAI APIを内部で使うため1回あたり10〜30秒程度かかる (ドキュメント記載値)。
const path = require('path');
const { execFile } = require('child_process');

const OCR_APP_DIR = process.env.OCR_APP_DIR || 'C:\\dev\\vcb_raspi_work';
const OCR_PYTHON_PATH =
  process.env.OCR_PYTHON_PATH || path.join(OCR_APP_DIR, '.venv_win', 'Scripts', 'python.exe');
const OCR_BRIDGE_SCRIPT = path.join(__dirname, '..', '..', 'tools', 'ocr_bridge.py');
const OCR_TIMEOUT_MS = 60_000;

// 戻り値は必ず {status, ...} を持つdict (呼び出し失敗時もstatus="error"に正規化する)。
function recognizeCounter(imagePath, equipmentId, capturedAtIso) {
  return new Promise((resolve) => {
    const args = [
      OCR_BRIDGE_SCRIPT,
      '--image', imagePath,
      '--app-dir', OCR_APP_DIR,
    ];
    if (equipmentId) args.push('--equipment-id', equipmentId);
    if (capturedAtIso) args.push('--captured-at', capturedAtIso);

    execFile(
      OCR_PYTHON_PATH,
      args,
      { timeout: OCR_TIMEOUT_MS, maxBuffer: 10 * 1024 * 1024 },
      (err, stdout, stderr) => {
        if (err) {
          resolve({ status: 'error', error: (stderr || err.message || '').toString().slice(0, 2000) });
          return;
        }
        try {
          resolve(JSON.parse(stdout));
        } catch (e) {
          resolve({ status: 'error', error: `OCR出力のJSON解析に失敗: ${e.message}` });
        }
      }
    );
  });
}

module.exports = { recognizeCounter };

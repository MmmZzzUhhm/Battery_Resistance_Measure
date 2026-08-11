"""
VCBカウンター認識(別チーム開発、C:\\dev\\vcb_raspi_work 相当)への橋渡し用CLI。

vcb_raspi_work自体は変更せず、公開インターフェースである
app.camera_interface.recognize_counter() をこのポータル側から呼び出すためだけの薄いラッパー。
--app-dir で指定されたパスをsys.pathに追加してインポートするため、カレントディレクトリに依存しない。

呼び出し元: portal/server/src/services/ocrClient.js
"""
import argparse
import json
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True)
    parser.add_argument("--app-dir", required=True, help="vcb_raspi_work のパス")
    parser.add_argument("--equipment-id")
    parser.add_argument("--captured-at")
    args = parser.parse_args()

    sys.path.insert(0, args.app_dir)
    from app.camera_interface import recognize_counter

    result = recognize_counter(
        image_path=args.image,
        equipment_id=args.equipment_id,
        captured_at=args.captured_at,
    )
    print(json.dumps(result, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()

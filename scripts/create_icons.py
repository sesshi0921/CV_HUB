import argparse
import os
from pathlib import Path
from PIL import Image


def create_icons(input_png: str, output_dir: str) -> None:
    source = Path(input_png)
    target_dir = Path(output_dir)

    if not source.exists():
        print(f"エラー: {source} が見つかりません。")
        return

    target_dir.mkdir(parents=True, exist_ok=True)
    img = Image.open(source)

    if img.size != (256, 256):
        print("警告: 画像サイズが256x256ではありません。リサイズします。")
        img = img.resize((256, 256), Image.Resampling.LANCZOS)

    png_path = target_dir / "app_icon.png"
    ico_path = target_dir / "app_icon.ico"
    icns_path = target_dir / "app_icon.icns"

    img.save(png_path, format="PNG")
    print(f"{png_path} を作成しました。")

    img.save(ico_path, format="ICO", sizes=[(256, 256)])
    print(f"{ico_path} を作成しました。")

    img.save(icns_path, format="ICNS")
    print(f"{icns_path} を作成しました。")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input_png", nargs="?", default="cv-hub.png")
    parser.add_argument("--output-dir", default="assets/icons")
    args = parser.parse_args()
    create_icons(args.input_png, args.output_dir)

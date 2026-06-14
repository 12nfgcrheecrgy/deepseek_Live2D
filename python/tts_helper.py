import sys
import json
import argparse
import asyncio
import os
import tempfile
import traceback

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

try:
    import edge_tts
except ImportError:
    print(json.dumps({
        "success": False,
        "error": "edge-tts not installed. Run: pip install edge-tts"
    }, ensure_ascii=False))
    sys.exit(1)


async def generate_tts(text, voice, rate, pitch, output_file):
    communicate = edge_tts.Communicate(
        text=text,
        voice=voice,
        rate=rate,
        pitch=pitch
    )
    await communicate.save(output_file)


def main():
    parser = argparse.ArgumentParser(description="Edge-TTS Helper")
    parser.add_argument("--text", required=True, help="Text to speak")
    parser.add_argument("--voice", default="zh-CN-XiaoxiaoNeural", help="Voice name")
    parser.add_argument("--rate", default="+0%", help="Speaking rate")
    parser.add_argument("--pitch", default="+0Hz", help="Pitch adjustment")
    parser.add_argument("--output", required=True, help="Output audio file path")

    args = parser.parse_args()

    try:
        output_path = os.path.abspath(args.output)
        output_dir = os.path.dirname(output_path)
        if output_dir:
            os.makedirs(output_dir, exist_ok=True)

        asyncio.run(generate_tts(
            text=args.text,
            voice=args.voice,
            rate=args.rate,
            pitch=args.pitch,
            output_file=output_path
        ))

        if os.path.exists(output_path) and os.path.getsize(output_path) > 0:
            result = {
                "success": True,
                "output_file": output_path,
                "size_bytes": os.path.getsize(output_path)
            }
        else:
            result = {
                "success": False,
                "error": "Generated file is empty or does not exist"
            }
        print(json.dumps(result, ensure_ascii=False))
    except Exception as e:
        result = {
            "success": False,
            "error": str(e),
            "traceback": traceback.format_exc()
        }
        print(json.dumps(result, ensure_ascii=False))
        sys.exit(1)


if __name__ == "__main__":
    main()
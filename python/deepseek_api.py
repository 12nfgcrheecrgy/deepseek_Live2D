import sys
import json
import argparse
import os
import traceback
from openai import OpenAI

sys.stdout.reconfigure(encoding='utf-8', errors='replace')


def chat_completion(api_key, api_base, model, messages, max_tokens, temperature):
    client = OpenAI(api_key=api_key, base_url=api_base)
    response = client.chat.completions.create(
        model=model,
        messages=messages,
        max_tokens=max_tokens,
        temperature=temperature,
        stream=False
    )
    return response.choices[0].message.content


def main():
    parser = argparse.ArgumentParser(description="DeepSeek API Client")
    parser.add_argument("--api-key", required=True, help="DeepSeek API Key")
    parser.add_argument("--api-base", default="https://api.deepseek.com/v1")
    parser.add_argument("--model", default="deepseek-chat")
    parser.add_argument("--max-tokens", type=int, default=1024)
    parser.add_argument("--temperature", type=float, default=0.8)
    parser.add_argument("--system-prompt", default="")
    parser.add_argument("--input-file", help="JSON file with user_message and screen_content")

    args = parser.parse_args()

    # Read messages from input file (avoids command-line escaping issues)
    user_message = ""
    screen_content = ""
    if args.input_file and os.path.exists(args.input_file):
        try:
            with open(args.input_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
                user_message = data.get('user_message', '')
                screen_content = data.get('screen_content', '')
        except Exception as e:
            print(json.dumps({"success": False, "error": f"Failed to read input file: {e}"},
                             ensure_ascii=False))
            sys.exit(1)

    if not user_message:
        print(json.dumps({"success": False, "error": "No user_message provided"},
                         ensure_ascii=False))
        sys.exit(1)

    messages = []
    if args.system_prompt:
        messages.append({"role": "system", "content": args.system_prompt})

    user_content = user_message
    if screen_content:
        user_content = f"[当前屏幕内容]:\n{screen_content}\n\n[用户输入]:\n{user_message}"

    messages.append({"role": "user", "content": user_content})

    try:
        reply = chat_completion(
            api_key=args.api_key,
            api_base=args.api_base,
            model=args.model,
            messages=messages,
            max_tokens=args.max_tokens,
            temperature=args.temperature
        )
        result = {"success": True, "reply": reply}
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

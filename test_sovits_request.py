import subprocess
import json
import os
import time

cmd = [r'Z:\deepseek_live2D\venv_tts\Scripts\python.exe',
       r'Z:\deepseek_live2D\bin\Release\python\sovits_server.py']

proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE, text=True, encoding='utf-8')

# Wait for ready response
start = time.time()
ready = False
while time.time() - start < 120:
    line = proc.stdout.readline().strip()
    if line:
        print("[OUT]", line)
        try:
            j = json.loads(line)
            if j.get('status') == 'ready':
                ready = True
                print("Server ready!")
                break
            elif j.get('status') == 'error':
                print("Server error:", j.get('error'))
                break
        except:
            pass
    if proc.poll() is not None:
        err = proc.stderr.read()
        print("Server exited early:", err[:2000])
        break

if not ready:
    print("Server did not become ready")
    proc.terminate()
    exit(1)

# Send synthesis request
req = {"text": "你好呀", "output": r"z:\test_sovits.wav", "emotion": "happy"}
proc.stdin.write(json.dumps(req, ensure_ascii=False) + "\n")
proc.stdin.flush()

# Read response
start = time.time()
while time.time() - start < 60:
    line = proc.stdout.readline().strip()
    if line:
        print("[RES]", line)
        try:
            j = json.loads(line)
            if j.get('success'):
                print("Synthesis succeeded! File:", j.get('output_file'))
                print("File size:", j.get('file_size'))
                break
            elif j.get('success') is False:
                print("Synthesis failed:", j.get('error'))
                break
        except:
            pass
    if proc.poll() is not None:
        break

# Shutdown
proc.stdin.write('{"command":"shutdown"}\n')
proc.stdin.flush()
proc.wait(timeout=10)
print("Done")

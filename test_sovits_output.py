import subprocess
import sys

cmd = [
    r"Z:\deepseek_live2D\venv_tts\Scripts\python.exe",
    r"Z:\deepseek_live2D\bin\Release\python\sovits_server.py"
]

# Test what sovits_server.py outputs on stdout/stderr during startup
proc = subprocess.Popen(
    cmd,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
    encoding='utf-8',
    errors='replace'
)

# Read stdout for up to 90 seconds
try:
    stdout_data, stderr_data = proc.communicate(timeout=90)
    print("=== STDOUT ===")
    print(stdout_data)
    print("=== STDERR ===")
    print(stderr_data)
    print(f"=== EXIT CODE: {proc.returncode} ===")
except subprocess.TimeoutExpired:
    proc.kill()
    stdout_data, stderr_data = proc.communicate()
    print("=== TIMEOUT (server still running) ===")
    print("=== STDOUT ===")
    print(stdout_data)
    print("=== STDERR ===")
    print(stderr_data)

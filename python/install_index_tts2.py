import subprocess
import sys
import os


MIRROR_ALIYUN = 'https://mirrors.aliyun.com/pypi/simple/'
PYTORCH_INDEX = 'https://download.pytorch.org/whl/cu121'
PYTHON312 = r'C:\Users\36314\AppData\Local\Programs\Python\Python312\python.exe'


def run_cmd(cmd, desc, cwd=None):
    print(f"\n{'='*60}")
    print(f"  {desc}")
    print(f"  {' '.join(cmd)}")
    print(f"{'='*60}")
    result = subprocess.run(cmd, capture_output=False, text=True, cwd=cwd)
    if result.returncode != 0:
        print(f"  [!] Warning: command returned {result.returncode}")
    else:
        print(f"  [OK] Done")
    return result.returncode == 0


def main():
    print("=" * 60)
    print("  IndexTTS2 + OpenVINO Installation Script")
    print("  GPU: NVIDIA GTX 1660 Super (6GB VRAM)")
    print("  Python: 3.12 venv + Domestic Mirrors")
    print("=" * 60)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    venv_dir = os.path.join(project_dir, 'venv_tts')
    model_dir = os.path.join(project_dir, 'models', 'indextts2')
    venv_python = os.path.join(venv_dir, 'Scripts', 'python.exe')
    venv_pip = os.path.join(venv_dir, 'Scripts', 'pip.exe')

    print(f"\nProject dir: {project_dir}")
    print(f"Venv dir:    {venv_dir}")
    print(f"Model dir:   {model_dir}")

    step = 1

    if not os.path.exists(venv_python):
        print(f"\n[Step {step}] Creating Python 3.12 virtual environment...")
        step += 1
        if not os.path.exists(PYTHON312):
            print(f"  [!] Python 3.12 not found at: {PYTHON312}")
            alt = r'C:\Python312\python.exe'
            if os.path.exists(alt):
                py312 = alt
            else:
                py312 = 'python3.12'
        else:
            py312 = PYTHON312
        run_cmd([py312, '-m', 'venv', venv_dir], 'Create venv')
    else:
        print(f"\n[Step {step}] Venv already exists: {venv_python}")
        step += 1

    print(f"\n[Step {step}] Configuring pip mirror (Aliyun)...")
    step += 1
    run_cmd([venv_python, '-m', 'pip', 'config', 'set', 'global.index-url', MIRROR_ALIYUN],
            'Set pip mirror')

    run_cmd([venv_python, '-m', 'pip', 'install', '--upgrade', 'pip'],
            'Upgrade pip')

    print(f"\n[Step {step}] Installing PyTorch CUDA 12.1...")
    step += 1
    run_cmd([venv_python, '-m', 'pip', 'install',
             'torch', 'torchvision', 'torchaudio',
             '--index-url', PYTORCH_INDEX],
            'PyTorch CUDA 12.1')

    print(f"\n[Step {step}] Installing core dependencies...")
    step += 1
    run_cmd([venv_python, '-m', 'pip', 'install',
             'transformers', 'accelerate', 'safetensors',
             'sentencepiece', 'numpy', 'scipy', 'librosa',
             'soundfile', 'pyyaml', 'tqdm', 'einops',
             'vector_quantize_pytorch', 'rotary_embedding_torch',
             'huggingface_hub', 'modelscope',
             'cython', 'cn2an', 'jieba', 'json5',
             'munch', 'ffmpeg-python', 'matplotlib',
             'keras', 'openvino', 'openvino-tokenizers'],
            'Core + OpenVINO deps')

    print(f"\n[Step {step}] Installing IndexTTS2 from GitHub (skip LFS)...")
    step += 1
    env = os.environ.copy()
    env['GIT_LFS_SKIP_SMUDGE'] = '1'
    result = subprocess.run(
        [venv_python, '-m', 'pip', 'install',
         'git+https://github.com/index-tts/index-tts.git'],
        capture_output=False, text=True, env=env
    )
    if result.returncode != 0:
        print("  [!] Git install failed, trying manual clone...")
        temp_clone = os.path.join(os.environ.get('TEMP', '.'), 'index-tts-clone')
        if os.path.exists(temp_clone):
            import shutil
            shutil.rmtree(temp_clone, ignore_errors=True)
        clone_result = subprocess.run(
            ['git', 'clone', '--depth', '1',
             'https://github.com/index-tts/index-tts.git', temp_clone],
            capture_output=False, text=True, env=env
        )
        if clone_result.returncode == 0:
            subprocess.run(
                [venv_python, '-m', 'pip', 'install', temp_clone],
                capture_output=False, text=True
            )
            import shutil
            shutil.rmtree(temp_clone, ignore_errors=True)

    if not os.path.exists(model_dir) or not os.path.exists(os.path.join(model_dir, 'config.yaml')):
        print(f"\n[Step {step}] Downloading IndexTTS2 models from ModelScope...")
        step += 1
        try:
            from modelscope import snapshot_download
            os.makedirs(model_dir, exist_ok=True)
            snapshot_download(
                model_id='IndexTeam/IndexTTS-2',
                local_dir=model_dir,
                revision='master'
            )
            print("  [OK] Models downloaded from ModelScope!")
        except Exception as e:
            print(f"  [ERR] Download failed: {e}")
    else:
        print(f"\n  [OK] Models already exist: {model_dir}")

    print(f"\n[Step {step}] Verifying installation...")
    step += 1
    verify_cmd = [venv_python, '-c',
                  "from indextts.infer_v2 import IndexTTS2; print('IndexTTS2 OK')"]
    result = subprocess.run(verify_cmd, capture_output=True, text=True)
    if 'IndexTTS2 OK' in result.stdout:
        print("  [OK] IndexTTS2 module loaded successfully!")
    else:
        print(f"  [WARN] IndexTTS2 import failed:")
        print(f"  stdout: {result.stdout}")
        print(f"  stderr: {result.stderr[:500]}")

    config_path = os.path.join(model_dir, 'config.yaml')
    if os.path.exists(config_path):
        print(f"  [OK] Config found: {config_path}")

    print(f"\n{'='*60}")
    print(f"  Installation complete!")
    print(f"  Venv:      {venv_python}")
    print(f"  Model dir: {model_dir}")
    print(f"")
    print(f"  Usage:")
    print(f"  1. Set tts.engine to 'indextts2' in config.json")
    print(f"  2. Add reference voice to music/reference_voice.wav")
    print(f"  3. The C++ app auto-detects the venv")
    print(f"  4. FP16 enabled by default for GTX 1660 Super")
    print(f"  5. OpenVINO optional for Intel CPU optimization")
    print(f"{'='*60}")


if __name__ == '__main__':
    main()
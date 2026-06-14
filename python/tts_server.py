import sys
import json
import os
import traceback
import time
import io
import re

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stdin.reconfigure(encoding='utf-8', errors='replace')


def log(msg):
    print(f"[tts_server] {msg}", file=sys.stderr, flush=True)


_progress_file = os.environ.get('TTS_PROGRESS_FILE', '')

def write_progress(percent, message, detail="", done=False, error=False, elapsed_sec=None):
    if not _progress_file:
        return
    try:
        data = {
            "percent": percent,
            "message": message,
            "detail": detail,
            "done": done,
            "error": error
        }
        if elapsed_sec is not None:
            data["elapsed_sec"] = round(elapsed_sec, 1)
        with open(_progress_file, 'w', encoding='utf-8') as f:
            json.dump(data, f, ensure_ascii=False)
    except Exception:
        pass


def send_response(data):
    line = json.dumps(data, ensure_ascii=False)
    sys.stdout.write(line + '\n')
    sys.stdout.flush()


def main():
    log("Starting TTS server...")
    write_progress(5, "正在启动IndexTTS2语音模型...", detail="检查GPU环境")

    if not torch.cuda.is_available():
        write_progress(0, "CUDA不可用，IndexTTS2需要NVIDIA GPU", error=True)
        send_response({
            'status': 'error',
            'error': 'CUDA not available. GPU required for IndexTTS2.'
        })
        return

    gpu_name = torch.cuda.get_device_name(0)
    total_vram = torch.cuda.get_device_properties(0).total_memory / (1024 ** 3)
    free_vram, _ = torch.cuda.mem_get_info()
    free_vram_gb = free_vram / (1024 ** 3)

    detail = f"GPU: {gpu_name} | 显存: {total_vram:.1f}GB (可用 {free_vram_gb:.1f}GB)"
    log(f"GPU: {gpu_name}, Total VRAM: {total_vram:.1f}GB, Free VRAM: {free_vram_gb:.1f}GB")
    write_progress(10, "检测到GPU，正在初始化...", detail=detail)

    if free_vram_gb < 4.0:
        write_progress(0, f"显存不足: 仅{free_vram_gb:.1f}GB可用，需要至少4GB", error=True)
        send_response({
            'status': 'error',
            'error': f'Insufficient VRAM: {free_vram_gb:.1f}GB free, need at least 4GB'
        })
        return

    log("Initializing GPU optimizations...")
    write_progress(15, "正在优化GPU设置...", detail=detail)
    torch.backends.cudnn.benchmark = True
    if hasattr(torch.backends.cuda.matmul, 'allow_tf32'):
        torch.backends.cuda.matmul.allow_tf32 = True

    try:
        from indextts.infer_v2 import IndexTTS2

        model_dir = None
        config_path = None
        reference_audio = None

        config_file = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'config.json')
        if os.path.exists(config_file):
            try:
                with open(config_file, 'r', encoding='utf-8') as f:
                    cfg = json.load(f)
                tts_cfg = cfg.get('tts', {})
                raw_model_dir = tts_cfg.get('indextts2_model_dir', '')
                if raw_model_dir:
                    if os.path.isabs(raw_model_dir):
                        model_dir = raw_model_dir
                    else:
                        project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
                        model_dir = os.path.join(project_root, raw_model_dir)
                config_path = tts_cfg.get('indextts2_config', '')
                if config_path and not os.path.isabs(config_path):
                    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
                    config_path = os.path.join(project_root, config_path)
                raw_ref = tts_cfg.get('indextts2_reference_audio', '')
                if raw_ref:
                    if os.path.isabs(raw_ref):
                        reference_audio = raw_ref
                    else:
                        project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
                        reference_audio = os.path.join(project_root, raw_ref)
            except Exception as e:
                log(f"Failed to read config.json: {e}")

        if not model_dir:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            parent_dir = os.path.dirname(script_dir)
            model_dir = os.path.join(parent_dir, 'models', 'indextts2')

        if not config_path:
            config_path = os.path.join(model_dir, 'config.yaml')

        if not os.path.exists(config_path):
            write_progress(0, f"配置文件未找到: {config_path}", error=True)
            send_response({
                'status': 'error',
                'error': f'Config file not found: {config_path}'
            })
            return

        log(f"Model dir: {model_dir}")
        log(f"Config: {config_path}")
        log(f"Reference audio: {reference_audio}")
        write_progress(20, "正在加载模型文件...", detail=f"模型目录: {os.path.basename(model_dir)}")

        load_start = time.time()

        def _elapsed():
            return time.time() - load_start

        write_progress(25, "正在解析模型配置...", detail=f"{os.path.basename(config_path)}", elapsed_sec=_elapsed())
        write_progress(28, "正在加载GPT声学模型...", detail="初始化IndexTTS2推理引擎", elapsed_sec=_elapsed())
        tts = IndexTTS2(
            cfg_path=config_path,
            model_dir=model_dir,
            use_fp16=True,
            use_cuda_kernel=True,
            use_deepspeed=False,
            use_accel=False,
            use_torch_compile=False
        )

        load_time = time.time() - load_start
        log(f"Models loaded in {load_time:.1f}s")
        write_progress(65, "模型加载完成，正在编译优化...", detail=f"耗时 {load_time:.1f}秒", elapsed_sec=load_time)

        try:
            log("Attempting torch.compile on GPT model...")
            write_progress(68, "正在编译GPT模型 (torch.compile)...", detail="reduce-overhead 模式", elapsed_sec=_elapsed())
            tts.gpt = torch.compile(tts.gpt, mode="reduce-overhead")
            log("torch.compile on GPT model OK")
        except Exception as e:
            log(f"torch.compile on GPT failed (will use eager mode): {e}")

        try:
            log("Attempting torch.compile on s2mel model...")
            write_progress(72, "正在编译s2mel模型 (torch.compile)...", detail="reduce-overhead 模式", elapsed_sec=_elapsed())
            tts.s2mel = torch.compile(tts.s2mel, mode="reduce-overhead")
            log("torch.compile on s2mel model OK")
        except Exception as e:
            log(f"torch.compile on s2mel failed (will use eager mode): {e}")

        log("Running warmup inference...")
        write_progress(85, "正在预热推理引擎...", detail="首次推理测试中", elapsed_sec=_elapsed())
        try:
            warmup_output = os.path.join(os.environ.get('TEMP', '.'), '_tts_warmup.wav')
            tts.infer(
                text='你好',
                output_path=warmup_output,
                verbose=False,
            )
            if os.path.exists(warmup_output):
                os.remove(warmup_output)
            log("Warmup completed")
        except Exception as e:
            log(f"Warmup failed (non-fatal): {e}")

        torch.cuda.empty_cache()
        _, after_vram = torch.cuda.mem_get_info()
        vram_used = (free_vram - after_vram) / (1024 ** 3)
        log(f"VRAM used after model load: {vram_used:.2f}GB")
        write_progress(95, "正在清理显存...", detail=f"显存占用: {vram_used:.2f}GB", elapsed_sec=_elapsed())

        send_response({
            'status': 'ready',
            'load_time_ms': round(load_time * 1000),
            'vram_used_gb': round(vram_used, 2),
            'gpu_name': gpu_name
        })
        write_progress(100, "IndexTTS2模型已就绪！", detail=f"{gpu_name} | 显存占用 {vram_used:.2f}GB", done=True)

        if not reference_audio or not os.path.exists(reference_audio):
            script_dir = os.path.dirname(os.path.abspath(__file__))
            parent_dir = os.path.dirname(script_dir)
            fallback = os.path.join(parent_dir, 'music', '5l51ir4quuzh6t4c7t906rhnnh6vida.mp3')
            if os.path.exists(fallback):
                reference_audio = fallback

        emotion_map = {
            'neutral': [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0],
            'happy': [0.8, 0.0, 0.0, 0.0, 0.0, 0.0, 0.2, 0.0],
            'sad': [0.0, 0.0, 0.7, 0.0, 0.0, 0.3, 0.0, 0.0],
            'surprised': [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.8, 0.0],
            'angry': [0.0, 0.8, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
            'calm': [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.9],
        }

        request_count = 0

        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue

            try:
                request = json.loads(line)
            except json.JSONDecodeError as e:
                log(f"Invalid JSON: {e}")
                send_response({'success': False, 'error': f'Invalid JSON: {e}'})
                continue

            command = request.get('command', '')
            if command == 'shutdown':
                log("Shutdown requested")
                send_response({'status': 'shutdown'})
                break

            if command == 'ping':
                send_response({'status': 'pong', 'request_count': request_count})
                continue

            if command == 'stream':
                text = request.get('text', '')
                output_dir = request.get('output_dir', '')
                emotion = request.get('emotion', 'neutral')

                if not text or not output_dir:
                    send_response({'success': False, 'error': 'Missing text or output_dir'})
                    continue

                try:
                    os.makedirs(output_dir, exist_ok=True)

                    def split_sentences(t):
                        t = re.sub(r'\s+', '', t)
                        s = re.split(r'(?<=[。！？.!?\n])', t)
                        s = [x.strip() for x in s if x.strip()]
                        if len(s) <= 1:
                            s = re.split(r'(?<=[，,、；;：:])', t)
                            s = [x.strip() for x in s if x.strip()]
                        return s

                    sentences = split_sentences(text)
                    if not sentences:
                        sentences = [text]

                    first_file = None
                    rest_files = []
                    synth_start = time.time()

                    emo_vector = emotion_map.get(emotion, emotion_map['neutral'])
                    spk_audio_prompt = reference_audio if reference_audio and os.path.exists(reference_audio) else None

                    for i, sentence in enumerate(sentences):
                        if not sentence.strip():
                            continue
                        chunk_path = os.path.join(output_dir, f"chunk_{i:04d}.wav")
                        tts_kwargs = {
                            'text': sentence,
                            'output_path': chunk_path,
                            'verbose': False,
                        }
                        if spk_audio_prompt:
                            tts_kwargs['spk_audio_prompt'] = spk_audio_prompt
                        tts.infer(**tts_kwargs)
                        if os.path.exists(chunk_path):
                            if first_file is None:
                                first_file = chunk_path
                            else:
                                rest_files.append(chunk_path)

                    synth_time = time.time() - synth_start
                    torch.cuda.empty_cache()
                    request_count += 1
                    send_response({
                        'success': True,
                        'first': first_file,
                        'rest': rest_files,
                        'synthesis_ms': round(synth_time * 1000)
                    })
                    log(f"Request #{request_count}: stream synthesized {len(rest_files) + 1} chunks in {synth_time:.2f}s")
                except Exception as e:
                    torch.cuda.empty_cache()
                    send_response({'success': False, 'error': f'{type(e).__name__}: {str(e)}'})
                    traceback.print_exc(file=sys.stderr)
                continue

            text = request.get('text', '')
            output_path = request.get('output', '')
            emotion = request.get('emotion', 'neutral')

            if not text or not output_path:
                send_response({'success': False, 'error': 'Missing text or output'})
                continue

            try:
                output_dir = os.path.dirname(output_path)
                if output_dir:
                    os.makedirs(output_dir, exist_ok=True)

                emo_vector = emotion_map.get(emotion, emotion_map['neutral'])

                synth_start = time.time()

                tts_kwargs = {
                    'text': text,
                    'output_path': output_path,
                    'verbose': False,
                }

                if reference_audio and os.path.exists(reference_audio):
                    tts_kwargs['spk_audio_prompt'] = reference_audio

                tts.infer(**tts_kwargs)

                synth_time = time.time() - synth_start

                if os.path.exists(output_path):
                    file_size = os.path.getsize(output_path)
                    torch.cuda.empty_cache()
                    request_count += 1
                    send_response({
                        'success': True,
                        'output_file': output_path,
                        'file_size': file_size,
                        'synthesis_ms': round(synth_time * 1000)
                    })
                    log(f"Request #{request_count}: synthesized {file_size} bytes in {synth_time:.2f}s")
                else:
                    send_response({
                        'success': False,
                        'error': 'Output file was not created'
                    })

            except Exception as e:
                torch.cuda.empty_cache()
                send_response({
                    'success': False,
                    'error': f'{type(e).__name__}: {str(e)}'
                })
                traceback.print_exc(file=sys.stderr)

        log("TTS server shutting down")
        del tts
        torch.cuda.empty_cache()

    except ImportError as e:
        write_progress(0, f"IndexTTS2未安装: {e}", error=True)
        send_response({
            'status': 'error',
            'error': f'IndexTTS2 not installed: {e}'
        })
    except Exception as e:
        err_msg = f"{type(e).__name__}: {str(e)}"
        write_progress(0, f"模型加载失败: {err_msg}", error=True)
        send_response({
            'status': 'error',
            'error': err_msg
        })
        traceback.print_exc(file=sys.stderr)


if __name__ == '__main__':
    import torch
    main()
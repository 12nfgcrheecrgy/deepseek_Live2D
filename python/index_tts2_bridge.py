import sys
import json
import argparse
import os
import traceback
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')


def main():
    parser = argparse.ArgumentParser(description='IndexTTS2 TTS Bridge')
    parser.add_argument('--text', required=True, help='Text to synthesize')
    parser.add_argument('--output', required=True, help='Output WAV file path')
    parser.add_argument('--model-dir', default=None, help='Model checkpoints directory')
    parser.add_argument('--config', default=None, help='Config YAML path')
    parser.add_argument('--reference-audio', default=None, help='Reference audio for voice cloning')
    parser.add_argument('--emotion', default='neutral', help='Emotion: neutral, happy, sad, surprised, angry, calm')
    parser.add_argument('--use-fp16', action='store_true', help='Use FP16 for faster GPU inference')
    parser.add_argument('--use-openvino', action='store_true', help='Use OpenVINO for Intel CPU/GPU optimization')
    parser.add_argument('--openvino-device', default='CPU', help='OpenVINO device: CPU, GPU')
    parser.add_argument('--speed', type=float, default=1.0, help='Speech speed multiplier')

    args = parser.parse_args()

    try:
        start_time = time.time()

        model_dir = args.model_dir
        config_path = args.config

        if not model_dir:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            parent_dir = os.path.dirname(script_dir)
            model_dir = os.path.join(parent_dir, 'models', 'indextts2')
        
        if not config_path:
            config_path = os.path.join(model_dir, 'config.yaml')

        if not os.path.exists(config_path):
            result = {
                'success': False,
                'error': f'Config file not found: {config_path}. Please download models first: python install_index_tts2.py'
            }
            print(json.dumps(result, ensure_ascii=True))
            return

        print(f"[IndexTTS2] Loading model from: {model_dir}", file=sys.stderr)
        print(f"[IndexTTS2] Config: {config_path}", file=sys.stderr)
        print(f"[IndexTTS2] FP16: {args.use_fp16}, OpenVINO: {args.use_openvino}", file=sys.stderr)

        if args.use_openvino:
            try:
                import openvino as ov
                print(f"[IndexTTS2] OpenVINO version: {ov.__version__}", file=sys.stderr)
                print(f"[IndexTTS2] OpenVINO device: {args.openvino_device}", file=sys.stderr)
            except ImportError:
                result = {
                    'success': False,
                    'error': 'OpenVINO not installed. Run: pip install openvino'
                }
                print(json.dumps(result, ensure_ascii=True))
                return

        from indextts.infer_v2 import IndexTTS2

        tts = IndexTTS2(
            cfg_path=config_path,
            model_dir=model_dir,
            use_fp16=args.use_fp16,
            use_cuda_kernel=False,
            use_deepspeed=False,
            use_accel=False,
            use_torch_compile=False
        )

        load_time = time.time() - start_time
        print(f"[IndexTTS2] Model loaded in {load_time:.1f}s", file=sys.stderr)

        output_dir = os.path.dirname(args.output)
        if output_dir:
            os.makedirs(output_dir, exist_ok=True)

        emotion_map = {
            'neutral': [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0],
            'happy': [0.8, 0.0, 0.0, 0.0, 0.0, 0.0, 0.2, 0.0],
            'sad': [0.0, 0.0, 0.7, 0.0, 0.0, 0.3, 0.0, 0.0],
            'surprised': [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.8, 0.0],
            'angry': [0.0, 0.8, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
            'calm': [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.9],
        }
        emo_vector = emotion_map.get(args.emotion, emotion_map['neutral'])

        tts_kwargs = {
            'text': args.text,
            'output_path': args.output,
            'verbose': False,
        }

        if args.reference_audio and os.path.exists(args.reference_audio):
            tts_kwargs['spk_audio_prompt'] = args.reference_audio
            print(f"[IndexTTS2] Voice cloning from: {args.reference_audio}", file=sys.stderr)
        else:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            parent_dir = os.path.dirname(script_dir)
            fallback_audio = os.path.join(parent_dir, 'music', '5l51ir4quuzh6t4c7t906rhnnh6vida.mp3')
            if os.path.exists(fallback_audio):
                tts_kwargs['spk_audio_prompt'] = fallback_audio
                print(f"[IndexTTS2] Using fallback audio: {fallback_audio}", file=sys.stderr)
            else:
                result = {'success': False, 'error': 'No reference audio provided and no fallback found'}
                print(json.dumps(result, ensure_ascii=True))
                return

        synth_start = time.time()
        tts.infer(**tts_kwargs)
        synth_time = time.time() - synth_start

        total_time = time.time() - start_time

        if os.path.exists(args.output):
            file_size = os.path.getsize(args.output)
            result = {
                'success': True,
                'output_file': args.output,
                'file_size': file_size,
                'model_load_ms': round(load_time * 1000),
                'synthesis_ms': round(synth_time * 1000),
                'total_ms': round(total_time * 1000)
            }
        else:
            result = {
                'success': False,
                'error': 'Output file was not created'
            }

        print(json.dumps(result, ensure_ascii=True))

    except ImportError as e:
        result = {
            'success': False,
            'error': f'IndexTTS2 not installed: {e}. Run: python install_index_tts2.py'
        }
        print(json.dumps(result, ensure_ascii=True))
    except Exception as e:
        result = {
            'success': False,
            'error': f'{type(e).__name__}: {str(e)}'
        }
        print(json.dumps(result, ensure_ascii=True))
        traceback.print_exc(file=sys.stderr)


if __name__ == '__main__':
    main()
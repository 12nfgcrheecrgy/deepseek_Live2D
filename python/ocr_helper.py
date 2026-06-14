import sys
import json
import argparse
import os
import traceback

# Ensure stdout can handle Chinese characters
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

try:
    from PIL import Image
except ImportError:
    print(json.dumps({
        "success": False,
        "error": "Pillow not installed. Run: pip install Pillow"
    }, ensure_ascii=False))
    sys.exit(1)

try:
    import win32gui
    import win32ui
    import win32con
    import win32api
except ImportError:
    print(json.dumps({
        "success": False,
        "error": "pywin32 not installed. Run: pip install pywin32"
    }, ensure_ascii=False))
    sys.exit(1)

# ── Windows built-in OCR (replaces Tesseract) ──────────────────────────
_WIN_OCR_AVAILABLE = False
_ocr_engine = None
try:
    import asyncio
    from winsdk.windows.media.ocr import OcrEngine, OcrResult
    from winsdk.windows.graphics.imaging import (
        SoftwareBitmap, BitmapPixelFormat, BitmapAlphaMode
    )
    from winsdk.windows.storage.streams import DataWriter
    from winsdk.windows.globalization import Language

    # Pre-create OCR engines for common languages
    _ocr_engines = {}

    def _get_ocr_engine(language_hint):
        """Get or create an OcrEngine for the given language hint."""
        if language_hint in _ocr_engines:
            return _ocr_engines[language_hint]

        # Map our language hints to BCP-47 tags
        lang_map = {
            'chi_sim': 'zh-Hans',
            'chi_sim+eng': 'zh-Hans',
            'zh': 'zh-Hans',
            'eng': 'en-US',
            'en': 'en-US',
            'ja': 'ja',
            'ko': 'ko',
        }
        bcp47 = lang_map.get(language_hint, 'zh-Hans')

        engine = OcrEngine.try_create_from_language(Language(bcp47))
        if not engine:
            # Fallback: use user profile languages
            engine = OcrEngine.try_create_from_user_profile_languages()
        if not engine:
            # Last resort: try any available language
            for lang in OcrEngine.get_available_recognizer_languages():
                engine = OcrEngine.try_create_from_language(lang)
                if engine:
                    break

        if engine:
            _ocr_engines[language_hint] = engine
        return engine

    _WIN_OCR_AVAILABLE = True
except ImportError:
    pass
except Exception as _e:
    import sys as _sys
    _WIN_OCR_AVAILABLE = False
    print(f"[ocr_helper] Windows OCR init failed: {type(_e).__name__}: {_e}", file=_sys.stderr)


def capture_full_screen():
    hdesktop = win32gui.GetDesktopWindow()
    left = win32api.GetSystemMetrics(win32con.SM_XVIRTUALSCREEN)
    top = win32api.GetSystemMetrics(win32con.SM_YVIRTUALSCREEN)
    width = win32api.GetSystemMetrics(win32con.SM_CXVIRTUALSCREEN)
    height = win32api.GetSystemMetrics(win32con.SM_CYVIRTUALSCREEN)

    desktop_dc = win32gui.GetWindowDC(hdesktop)
    img_dc = win32ui.CreateDCFromHandle(desktop_dc)
    mem_dc = img_dc.CreateCompatibleDC()

    screenshot = win32ui.CreateBitmap()
    screenshot.CreateCompatibleBitmap(img_dc, width, height)
    mem_dc.SelectObject(screenshot)

    mem_dc.BitBlt((0, 0), (width, height), img_dc, (left, top), win32con.SRCCOPY)

    bmpinfo = screenshot.GetInfo()
    bmpstr = screenshot.GetBitmapBits(True)

    img = Image.frombuffer(
        'RGB',
        (bmpinfo['bmWidth'], bmpinfo['bmHeight']),
        bmpstr, 'raw', 'BGRX', 0, 1
    )

    mem_dc.DeleteDC()
    img_dc.DeleteDC()
    win32gui.ReleaseDC(hdesktop, desktop_dc)
    win32gui.DeleteObject(screenshot.GetHandle())

    return img


def capture_screen(x, y, w, h):
    hdesktop = win32gui.GetDesktopWindow()
    desktop_dc = win32gui.GetWindowDC(hdesktop)
    img_dc = win32ui.CreateDCFromHandle(desktop_dc)
    mem_dc = img_dc.CreateCompatibleDC()

    screenshot = win32ui.CreateBitmap()
    screenshot.CreateCompatibleBitmap(img_dc, w, h)
    mem_dc.SelectObject(screenshot)

    mem_dc.BitBlt((0, 0), (w, h), img_dc, (x, y), win32con.SRCCOPY)

    bmpinfo = screenshot.GetInfo()
    bmpstr = screenshot.GetBitmapBits(True)

    img = Image.frombuffer(
        'RGB',
        (bmpinfo['bmWidth'], bmpinfo['bmHeight']),
        bmpstr, 'raw', 'BGRX', 0, 1
    )

    mem_dc.DeleteDC()
    img_dc.DeleteDC()
    win32gui.ReleaseDC(hdesktop, desktop_dc)
    win32gui.DeleteObject(screenshot.GetHandle())

    return img


def ocr_from_image_windows(image, language):
    """Use Windows built-in OCR (Windows.Media.Ocr)."""
    # Initialize COM / WinRT on this thread
    try:
        import pythoncom
        pythoncom.CoInitialize()
    except ImportError:
        import ctypes
        # COINIT_MULTITHREADED for WinRT compatibility
        ctypes.windll.ole32.CoInitializeEx(None, 0)
    # Also initialize Windows Runtime
    try:
        import ctypes as _ct
        _ct.windll.ole32.CoInitializeEx(None, 0)
    except Exception:
        pass

    engine = _get_ocr_engine(language)
    if not engine:
        raise RuntimeError("No Windows OCR engine available. "
                           "Install a language pack in Settings > Language.")

    # Convert PIL image to BGRA8 for Windows OCR
    # PIL stores RGBA, Windows SoftwareBitmap expects BGRA8
    if image.mode != 'RGBA':
        image = image.convert('RGBA')
    
    # Swap R and B to convert RGBA -> BGRA
    raw = bytearray(image.tobytes())
    for i in range(0, len(raw), 4):
        raw[i], raw[i + 2] = raw[i + 2], raw[i]

    width, height = image.size

    # Create SoftwareBitmap via DataWriter
    writer = DataWriter()
    writer.write_bytes(bytes(raw))
    buf = writer.detach_buffer()
    bitmap = SoftwareBitmap.create_copy_from_buffer(
        buf, BitmapPixelFormat.BGRA8, width, height, BitmapAlphaMode.STRAIGHT
    )

    # Run OCR via asyncio (WinRT async API)
    async def _do_ocr():
        return await engine.recognize_async(bitmap)
    result = asyncio.run(_do_ocr())

    # Extract text
    text = result.text
    return text.strip() if text else ""


def ocr_from_image_tesseract(image, language):
    """Fallback: use Tesseract OCR."""
    try:
        import pytesseract
    except ImportError:
        raise RuntimeError("Neither Windows OCR nor Tesseract is available.")
    custom_config = f'--oem 3 --psm 6 -l {language}'
    text = pytesseract.image_to_string(image, config=custom_config)
    return text.strip()


def ocr_from_image(image, language):
    """OCR the given PIL image, preferring Windows OCR over Tesseract."""
    if _WIN_OCR_AVAILABLE:
        try:
            return ocr_from_image_windows(image, language)
        except Exception as _ocr_err:
            # Windows OCR failed, try Tesseract fallback
            import sys as _sys
            print(f"[ocr_helper] Windows OCR failed: {_ocr_err}", file=_sys.stderr)
    return ocr_from_image_tesseract(image, language)


def main():
    parser = argparse.ArgumentParser(description="OCR Helper (Windows built-in OCR)")
    parser.add_argument("--language", default="chi_sim+eng", help="OCR language")
    parser.add_argument("--mode", choices=["full", "region"], default="full",
                        help="Capture mode: full screen or region")
    parser.add_argument("--x", type=int, default=0, help="Region X")
    parser.add_argument("--y", type=int, default=0, help="Region Y")
    parser.add_argument("--width", type=int, default=1920, help="Region width")
    parser.add_argument("--height", type=int, default=1080, help="Region height")
    parser.add_argument("--output-text", default="", help="Save OCR result to file")

    args = parser.parse_args()

    try:
        if args.mode == "full":
            img = capture_full_screen()
        else:
            img = capture_screen(args.x, args.y, args.width, args.height)

        text = ocr_from_image(img, args.language)

        if args.output_text:
            with open(args.output_text, 'w', encoding='utf-8') as f:
                f.write(text)

        result = {
            "success": True,
            "text": text,
            "text_length": len(text)
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

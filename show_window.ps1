Add-Type @"
using System;
using System.Runtime.InteropServices;
public class WinAPI {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hWnd, int x, int y, int w, int h, bool repaint);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int x, int y, int cx, int cy, uint flags);
  public static readonly IntPtr HWND_TOPMOST = new IntPtr(-1);
  public static readonly IntPtr HWND_NOTOPMOST = new IntPtr(-2);
  public static readonly IntPtr HWND_TOP = new IntPtr(0);
  public const uint SWP_SHOWWINDOW = 0x0040;
  public const uint SWP_NOSIZE = 0x0001;
  public const uint SWP_NOMOVE = 0x0002;
}
"@

$ps = Get-Process Live2DCompanion -ErrorAction SilentlyContinue
if (-not $ps) {
    Write-Host "Process not found"
    exit 1
}

$hwnd = $ps.MainWindowHandle
Write-Host "HWND: $hwnd"
Write-Host "Visible: $([WinAPI]::IsWindowVisible($hwnd))"
Write-Host "Iconic: $([WinAPI]::IsIconic($hwnd))"

# Restore if minimized
[WinAPI]::ShowWindow($hwnd, 9)
# Move to visible area (center of primary screen)
[WinAPI]::MoveWindow($hwnd, 760, 290, 400, 500, $true)
# Set topmost then normal
[WinAPI]::SetWindowPos($hwnd, [WinAPI]::HWND_TOPMOST, 0, 0, 0, 0, 0x0040 -bor 0x0001 -bor 0x0002)
Start-Sleep -Milliseconds 200
[WinAPI]::SetWindowPos($hwnd, [WinAPI]::HWND_TOP, 0, 0, 0, 0, 0x0040 -bor 0x0001 -bor 0x0002)
# Force foreground
[WinAPI]::SetForegroundWindow($hwnd)

Write-Host "Window should now be visible at center screen"

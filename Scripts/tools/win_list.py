"""列出可见顶层窗口（含 hwnd / pid / 标题），用于定位 PIE 客户端窗口。

为什么需要它：PIE 三开是同进程多窗口（RunUnderOneProcess），tasklist 只能看到主窗口标题，
要按窗口投键必须拿到各窗口的 HWND。用 ctypes 走 user32，不依赖 PowerShell 的 Add-Type
（后者在本机被安全策略禁止）。

用法：python Scripts/tools/win_list.py [关键字]        关键字缺省为 "Tank|Unreal|PIE"
"""

import ctypes
import re
import sys
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)

EnumWindowsProc = ctypes.WINFUNCTYPE(
    wintypes.BOOL, wintypes.HWND, wintypes.LPARAM
)

user32.EnumWindows.argtypes = [EnumWindowsProc, wintypes.LPARAM]
user32.EnumWindows.restype = wintypes.BOOL
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.IsWindowVisible.argtypes = [wintypes.HWND]
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]

pattern = re.compile(sys.argv[1] if len(sys.argv) > 1 else r"Tank|Unreal|PIE", re.I)
hits = []


def _cb(hwnd, _lparam):
    if not user32.IsWindowVisible(hwnd):
        return True
    buf = ctypes.create_unicode_buffer(512)
    user32.GetWindowTextW(hwnd, buf, 512)
    title = buf.value
    if title and pattern.search(title):
        pid = wintypes.DWORD(0)
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        hits.append((hwnd, pid.value, title))
    return True


user32.EnumWindows(EnumWindowsProc(_cb), 0)

print("命中 %d 个窗口：" % len(hits))
for hwnd, pid, title in hits:
    print("  hwnd=0x%08X pid=%-7d %s" % (hwnd, pid, title))

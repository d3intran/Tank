"""向指定窗口投一次鼠标点击（用来把 UE 的 PIE 视口点成焦点）。

为什么需要：UE 编辑器里 PIE 视口要先有键盘焦点，投递的 WM_KEYDOWN 才会进游戏输入；
`win_key.py --activate` 只改 Slate 的窗口簿记，视口控件本身没拿到焦点时按键会被丢。
点击点用窗口客户区中心（避开左上角工具条与右下角小部件）。
"""

import ctypes
import sys
import time
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
user32.EnumWindows.argtypes = [ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM), wintypes.LPARAM]
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.IsWindowVisible.argtypes = [wintypes.HWND]
user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user32.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
user32.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
user32.mouse_event.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]

MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004

title_sub = sys.argv[1] if len(sys.argv) > 1 else "Tank - Unreal Editor"
x_ratio = float(sys.argv[2]) if len(sys.argv) > 2 else 0.5
y_ratio = float(sys.argv[3]) if len(sys.argv) > 3 else 0.5

hits = []


def cb(hwnd, _lparam):
    t = ctypes.create_unicode_buffer(512)
    user32.GetWindowTextW(hwnd, t, 512)
    if user32.IsWindowVisible(hwnd) and title_sub.lower() in t.value.lower():
        hits.append(hwnd)
    return True


user32.EnumWindows(ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)(cb), 0)
if not hits:
    print("没找到窗口: %s" % title_sub)
    sys.exit(1)

hwnd = hits[0]
rect = wintypes.RECT()
user32.GetClientRect(hwnd, ctypes.byref(rect))
pt = wintypes.POINT(int((rect.right - rect.left) * x_ratio), int((rect.bottom - rect.top) * y_ratio))
user32.ClientToScreen(hwnd, ctypes.byref(pt))
user32.SetCursorPos(int(pt.x), int(pt.y))
time.sleep(0.15)
user32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, None)
time.sleep(0.05)
user32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, None)
print("已在窗口 '%s' 客户区中心 (%d, %d) 点击" % (title_sub, pt.x, pt.y))

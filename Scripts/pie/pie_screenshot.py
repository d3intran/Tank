"""对 PIE 画面截图，用于人工核对 HUD 渲染。

UE 的 HighResShot / Shot 走 GEditor 的 active viewport，多窗口 PIE 下不一定命中 PIE 窗口，
所以这里把几种可行方式都试一遍并报告结果，最后统一去 Saved/Screenshots 里找新文件。
"""

import unreal
import tank_shot  # 持有式截图，见 Content/Python/tank_shot.py
import os
import glob

out.clear()

shot_dir = os.path.join(unreal.Paths.project_saved_dir(), "Screenshots")
before = set(glob.glob(os.path.join(shot_dir, "**", "*.png"), recursive=True))
out.append("截图前已有 png: %d" % len(before))

# PIE 世界（拿它当 world context，尽量让命令落在 PIE 而不是编辑器视口）
pie_world = None
try:
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            pie_world = w
            break
except Exception as exc:  # noqa: BLE001
    out.append("取 PIE 世界失败: %s" % exc)

ctx = pie_world
if ctx is None:
    ctx = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    out.append("未取到 PIE 世界，退回编辑器世界（截图可能拍到编辑器视口）")
else:
    out.append("使用 PIE 服务端世界作 context: %s" % ctx.get_name())

# 方式 A：HighResShot
try:
    unreal.SystemLibrary.execute_console_command(ctx, "HighResShot 1600x900")
    out.append("A) HighResShot 1600x900 已发出")
except Exception as exc:  # noqa: BLE001
    out.append("A) HighResShot 失败: %s" % exc)

# 方式 B：Shot（普通截图）
try:
    unreal.SystemLibrary.execute_console_command(ctx, "Shot")
    out.append("B) Shot 已发出")
except Exception as exc:  # noqa: BLE001
    out.append("B) Shot 失败: %s" % exc)

# 方式 C：AutomationLibrary（若该版本有）
if hasattr(unreal, "AutomationLibrary") and hasattr(unreal.AutomationLibrary, "take_high_res_screenshot"):
    try:
        tank_shot.shot("hud_check.png")
        out.append("C) AutomationLibrary.take_high_res_screenshot 已调用")
    except Exception as exc:  # noqa: BLE001
        out.append("C) 失败: %s" % exc)
else:
    out.append("C) 无 AutomationLibrary.take_high_res_screenshot")

out.append("")
out.append("截图是异步落盘的，稍后再列目录。")

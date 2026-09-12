"""探测：编辑器内可用的 PIE 启动 API + 记录当前日志位置（为跑一局做准备）。"""

import unreal

out.clear()

out.append("========== LevelEditorSubsystem 方法 ==========")
try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    names = [n for n in dir(les) if not n.startswith("_")]
    out.append(", ".join(names))
except Exception as exc:  # noqa: BLE001
    out.append("取子系统失败: %s" % exc)

out.append("")
out.append("========== unreal 模块里含 'play' 的成员 ==========")
hits = [n for n in dir(unreal) if "play" in n.lower()]
out.append(", ".join(hits))

out.append("")
out.append("========== 含 'pie' 的成员 ==========")
hits = [n for n in dir(unreal) if "pie" in n.lower()]
out.append(", ".join(hits))

out.append("")
out.append("========== 与 PIE 设置相关的类 ==========")
for n in ("LevelEditorPlaySettings", "EditorPerformanceProjectSettings", "EditorLevelLibrary",
          "EditorLoadingAndSavingUtils", "UnrealEditorSubsystem", "EditorAssetLibrary"):
    out.append("  %-32s %s" % (n, "有" if hasattr(unreal, n) else "无"))

out.append("")
out.append("========== UnrealEditorSubsystem 方法（找 play / sim 相关）==========")
try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    names = [n for n in dir(ues) if not n.startswith("_")]
    out.append(", ".join(names))
except Exception as exc:  # noqa: BLE001
    out.append("取子系统失败: %s" % exc)

out.append("")
out.append("========== EditorLevelLibrary（旧 API，可能带 editor_play_*）==========")
try:
    ell = unreal.EditorLevelLibrary
    names = [n for n in dir(ell) if not n.startswith("_")]
    out.append(", ".join(names))
except Exception as exc:  # noqa: BLE001
    out.append("失败: %s" % exc)

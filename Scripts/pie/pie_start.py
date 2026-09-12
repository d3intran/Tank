"""启动 PIE（沿用已保存的设置：ListenServer + RunUnderOneProcess + 3 客户端）。"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out.append("is_in_play_in_editor (before) = %s" % les.is_in_play_in_editor())

les.editor_request_begin_play()
out.append("editor_request_begin_play() 已调用")

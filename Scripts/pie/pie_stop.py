"""结束 PIE。"""

import unreal

out.clear()
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out.append("is_in_play_in_editor (before) = %s" % les.is_in_play_in_editor())
les.editor_request_end_play()
out.append("editor_request_end_play() 已调用")

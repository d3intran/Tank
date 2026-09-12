"""探测：能否从 Python 注入 Enhanced Input，从而让坦克真的动起来（用于验证远端负重轮）。"""

import unreal

out.clear()

out.append("EnhancedInputLocalPlayerSubsystem 存在: %s" % hasattr(unreal, "EnhancedInputLocalPlayerSubsystem"))
if hasattr(unreal, "EnhancedInputLocalPlayerSubsystem"):
    ms = [n for n in dir(unreal.EnhancedInputLocalPlayerSubsystem) if not n.startswith("_")]
    out.append("  含 inject/input 的方法: %s" % ", ".join(n for n in ms if "inject" in n.lower() or "input" in n.lower()))

out.append("InputActionValue 存在: %s" % hasattr(unreal, "InputActionValue"))
out.append("InputAction 存在: %s" % hasattr(unreal, "InputAction"))

ia = unreal.EditorAssetLibrary.load_asset("/Game/tank/inputs/IA_MoveForward.IA_MoveForward")
out.append("IA_MoveForward = %s" % (ia.get_name() if ia else None))

# 取 LocalPlayer 的几种途径
ws = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
pc = unreal.GameplayStatics.get_player_controller(ws, 0)
out.append("")
out.append("编辑器世界 PC = %s" % (pc.get_name() if pc else None))
if pc:
    for meth in ("get_local_player", "get_localplayer"):
        out.append("  pc.%s 存在: %s" % (meth, hasattr(pc, meth)))

gi = unreal.GameplayStatics.get_game_instance(ws)
out.append("GameInstance = %s" % (gi.get_name() if gi else None))
if gi:
    for meth in ("get_local_player", "get_num_local_players"):
        out.append("  gi.%s 存在: %s" % (meth, hasattr(gi, meth)))
    if hasattr(gi, "get_num_local_players"):
        out.append("  本地玩家数 = %s" % gi.get_num_local_players())
    if hasattr(gi, "get_local_player"):
        lp = gi.get_local_player(0)
        out.append("  lp = %s" % (lp.get_name() if lp else None))
        if lp:
            out.append("  lp.get_subsystem 存在: %s" % hasattr(lp, "get_subsystem"))

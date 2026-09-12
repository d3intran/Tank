"""探测 3：找一条 Python 可达的「给某个 PlayerController 注入 Enhanced Input」路径。

背景：ATankPawn 的输入回调未反射，验证客户端移动只能靠 Enhanced Input 注入；
而注入需要在 LocalPlayer 子系统上做。ULocalPlayer::GetSubsystem 是模板（不可达），
所以要找别的入口。顺带确认 Tick 相关的可读状态（机制层的直接证据）。
"""

import unreal

out.clear()


def has(name):
    return hasattr(unreal, name)


def members(obj, *keys):
    names = [n for n in dir(obj) if not n.startswith("_")]
    return sorted(n for n in names if any(k.lower() in n.lower() for k in keys))


out.append("=== unreal 顶层含 Subsystem 的名字 ===")
out.append("  %s" % ", ".join(members(unreal, "Subsystem")))

out.append("")
out.append("=== SubsystemBlueprintLibrary ===")
out.append("  存在: %s" % has("SubsystemBlueprintLibrary"))
if has("SubsystemBlueprintLibrary"):
    out.append("  方法: %s" % ", ".join(members(unreal.SubsystemBlueprintLibrary, "subsystem")))

out.append("")
out.append("=== PlayerController 上含 local/player/subsystem 的成员 ===")
out.append("  %s" % ", ".join(members(unreal.PlayerController, "localplayer", "player", "subsystem")))

out.append("")
out.append("=== EnhancedInputLocalPlayerSubsystem 的注入方法 ===")
out.append("  %s" % ", ".join(members(unreal.EnhancedInputLocalPlayerSubsystem, "inject")))

out.append("")
out.append("=== EnhancedInputComponent 的注入方法 ===")
out.append("  %s" % ", ".join(members(unreal.EnhancedInputComponent, "inject", "bind", "action")))

out.append("")
out.append("=== Actor/Pawn 的 tick 可读状态 ===")
for cls in (unreal.Actor, unreal.Pawn):
    out.append("  %s: %s" % (cls.__name__, ", ".join(members(cls, "tick"))))

out.append("")
out.append("=== 输入动作资产是否可加载（unreal.load_asset） ===")
ia = unreal.load_asset("/Game/tank/inputs/IA_MoveForward")
out.append("  IA_MoveForward -> %s (%s)" % (ia, type(ia).__name__ if ia else "n/a"))
out.append("  IMC -> %s" % unreal.load_asset("/Game/tank/inputs/IMC_Tank"))

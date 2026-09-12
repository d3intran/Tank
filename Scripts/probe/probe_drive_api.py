"""一次性把「驱动坦克 / 制造孤儿」相关的可用 Python API 列出来，避免反复试错。"""

import unreal

out.clear()

server = None
for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
    pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
    if len(pcs) > 1:
        server = w
        break

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if server is None or not les.is_in_play_in_editor():
    out.append("PIE 未运行（本探针需要 PIE 才能取到 GameInstance/PC）")
else:
    gi = unreal.GameplayStatics.get_game_instance(server)
    out.append("GameInstance = %s" % (gi.get_name() if gi else None))
    if gi is not None:
        out.append("  GameInstance 含 player 的成员: %s"
                   % ", ".join(m for m in dir(gi) if "player" in m.lower()))

    pcs = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController)
    pc = pcs[0]
    out.append("")
    out.append("PC 类 = %s" % pc.get_class().get_name())
    out.append("  PC 含 possess/pawn 的成员: %s"
               % ", ".join(m for m in dir(pc) if "possess" in m.lower() or "pawn" in m.lower()))

    # LocalPlayer 及其子系统
    lp = None
    if gi is not None:
        for meth in ("get_first_game_player", "get_local_player", "get_local_players"):
            f = getattr(gi, meth, None)
            if f is None:
                continue
            try:
                r = f(0) if meth == "get_local_player" else f()
                if isinstance(r, (list, tuple)):
                    r = r[0] if r else None
                out.append("  gi.%s() -> %s" % (meth, r))
                if r is not None and lp is None:
                    lp = r
            except Exception as exc:  # noqa: BLE001
                out.append("  gi.%s() 失败: %s" % (meth, exc))

    if lp is not None:
        out.append("")
        out.append("LocalPlayer = %s" % lp.get_name())
        out.append("  含 subsystem 的成员: %s"
                   % ", ".join(m for m in dir(lp) if "subsystem" in m.lower()))

    # 编辑器/引擎层面有没有直接拿本地玩家的工具
    out.append("")
    out.append("unreal 里含 localplayer 的成员: %s"
               % ", ".join(m for m in dir(unreal) if "localplayer" in m.lower()))

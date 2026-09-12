"""制造一辆「无 Controller 的孤儿坦克」——它既不是本机受控，也没有客户端会覆盖它的位置，
正好用来单独测试负重轮的「远端从位移反推」路径。

注意：孤儿坦克是**故意造出来的测试对象**，不是游戏 bug——
M2 修复后正常流程不会产生孤儿，这里手动 unpossess 一个客户端 PC 来造一个。
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    server = None
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            server = w
            break

    if server is None:
        out.append("没找到服务端世界")
    else:
        pcs = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController)
        host = None
        others = []
        for pc in pcs:
            f = getattr(pc, "is_local_player_controller", None)
            ok = False
            if f is not None:
                try:
                    ok = bool(f())
                except Exception:  # noqa: BLE001
                    pass
            (others.append(pc) if not ok else None)
            if ok:
                host = pc

        if not others:
            out.append("没有非主机 PC 可用来造孤儿")
        else:
            victim_pc = others[0]
            out.append("对 %s 执行 unpossess（其坦克将变成孤儿，供测试用）" % victim_pc.get_name())
            try:
                victim_pc.unpossess()
                out.append("unpossess 已执行")
            except Exception as exc:  # noqa: BLE001
                out.append("unpossess 失败: %s" % exc)
            out.append("（GameMode 巡检会在 2s 内给该 PC 补发新坦克，旧坦克留作孤儿）")

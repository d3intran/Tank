"""把服务端世界里所有坦克一次性打死，验证三名玩家（含主机）的重生占有链路。

比 pie_kill.py 更彻底：主机自己的重生路径（PC->Pawn 销毁 → 巡检 RestartPlayer）
也一并覆盖，且三辆同时死可以压一压同一帧内的多次 Spawn。
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未在运行——先跑 pie_start.py")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = None
    for w in worlds:
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            server = w
            break

    if server is None:
        out.append("没找到服务端世界")
    else:
        tanks = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn)
        out.append("服务端世界 %s：准备击杀 %d 辆" % (server.get_name(), len(tanks)))
        for t in tanks:
            c = t.get_controller()
            out.append("  击杀 %s (controller=%s)" % (t.get_name(), c.get_name() if c else "NONE"))
            unreal.GameplayStatics.apply_damage(t, 9999.0, c, t, None)
        out.append("全部施加 9999 伤害完成，等待巡检补发…")

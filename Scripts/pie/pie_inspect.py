"""修正版 PIE 检查。

上一版把「客户端世界里远端 Pawn 的 Controller 为空」误判成孤儿坦克——
实际上远端 PlayerController 根本不会复制到客户端，客户端上只有本地玩家的
Pawn 有 Controller，这是引擎的正常行为。

所以孤儿判定**只在服务端世界（唯一拥有全部 3 个 PlayerController 的那个世界）**才有意义。
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未在运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    out.append("PIE 世界数 = %d（ListenServer + 3 客户端，期望 4 = 1 server + 3 client）" % len(worlds))
    out.append("")

    server_world = None
    rows = []
    for w in worlds:
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        tanks = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.TankPawn)
        rows.append((w, len(pcs), tanks))
        if server_world is None or len(pcs) > unreal.GameplayStatics.get_all_actors_of_class(
                server_world, unreal.PlayerController).__len__():
            server_world = w

    for w, npc, tanks in rows:
        tag = "  <== 服务端世界" if w == server_world else "  (客户端世界)"
        out.append("=== %s : PC=%d Tank=%d%s" % (w.get_name(), npc, len(tanks), tag))
        locs = []
        for t in tanks:
            ctrl = t.get_controller()
            loc = t.get_actor_location()
            locs.append(loc)
            out.append("    %-14s loc=(%.0f,%.0f,%.0f) controller=%s"
                       % (t.get_name(), loc.x, loc.y, loc.z,
                          ctrl.get_name() if ctrl else "NONE"))
        for i in range(len(locs)):
            for j in range(i + 1, len(locs)):
                d = (locs[i] - locs[j]).length()
                if d < 200.0:
                    out.append("    !! 堆叠：相距 %.0fcm" % d)
        out.append("")

    # ---- 只在服务端世界做孤儿断言 ----
    out.append("========== 服务端世界断言 ==========")
    s_tanks = unreal.GameplayStatics.get_all_actors_of_class(server_world, unreal.TankPawn)
    s_pcs = unreal.GameplayStatics.get_all_actors_of_class(server_world, unreal.PlayerController)
    orphans = [t for t in s_tanks if t.get_controller() is None]
    out.append("服务端世界: Tank=%d PC=%d" % (len(s_tanks), len(s_pcs)))
    for t in s_tanks:
        c = t.get_controller()
        out.append("   %s -> %s" % (t.get_name(), c.get_name() if c else "NONE"))
    out.append(">>> 断言 服务端无孤儿坦克 : %s (孤儿 %d 辆)"
               % ("PASS" if len(orphans) == 0 else "FAIL", len(orphans)))

    # PC 的 Pawn 归属（用反射属性，避开 get_pawn 可能不存在的问题）
    out.append("")
    out.append("PC -> Pawn 归属（读反射属性 pawn）：")
    for pc in s_pcs:
        try:
            p = pc.get_editor_property("pawn")
            pn = p.get_name() if p else "NONE"
        except Exception as exc:  # noqa: BLE001
            pn = "<读取失败: %s>" % exc
        out.append("   %-20s pawn=%s" % (pc.get_name(), pn))

"""M4b 调试探针：本机车脚下的地面探测到底打到了什么（复现 C++ UpdateGroundContact 的射线）。"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))

    # 找服务器世界里「本机」的那辆车（主机自己的车）
    tank = None
    for pc in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                tank = pc.get_controlled_pawn()
                break
        except Exception:  # noqa: BLE001
            pass
    if tank is None:
        out.append("没找到主机本机车")
    else:
        loc = tank.get_actor_location()
        out.append("本机车 %s loc=(%.1f, %.1f, %.1f)" % (tank.get_name(), loc.x, loc.y, loc.z))

        # 与 C++ 相同：盒底往上 10cm 起测，向下 10+60cm
        half = 59.0
        start = unreal.Vector(loc.x, loc.y, loc.z - half + 10.0)
        end = unreal.Vector(loc.x, loc.y, loc.z - half - 70.0)
        out.append("探测: %.1f → %.1f" % (start.z, end.z))

        for label, chan in (("Visibility", unreal.TraceTypeQuery.ECC_VISIBILITY),
                            ("Camera", unreal.TraceTypeQuery.ECC_CAMERA),
                            ("WorldStatic", unreal.TraceTypeQuery.TRACE_TYPE_QUERY1)):
            hit = unreal.GameplayStatics.trace_single(server, chan, start, end, False, None, None, True)
            if hit is None:
                out.append("  [%s] 无命中" % label)
                continue
            hb = hit.to_tuple()
            blocking, h_loc, h_norm = hb[0], hb[4], hb[5]
            actor = hb[11] if len(hb) > 11 else None
            out.append("  [%s] 命中 Z=%.1f  法线=(%.2f,%.2f,%.2f)  Actor=%s"
                       % (label, h_loc.z, h_norm.x, h_norm.y, h_norm.z,
                          actor.get_name() if actor else "None"))

        # 2 秒内 Z 是否稳定（判断是不是悬空/还在落）
        z0 = tank.get_actor_location().z
        unreal.SystemLibrary.delay(server, 1.5)
        z1 = tank.get_actor_location().z
        out.append("")
        out.append("1.5s 后 Z: %.1f → %.1f（差 %.2f）" % (z0, z1, z1 - z0))

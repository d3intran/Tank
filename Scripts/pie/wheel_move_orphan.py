"""把孤儿坦克沿自身前方瞬移一段距离，然后看它的负重轮有没有跟着转。

这是「远端负重轮」修复的关键判据：
  孤儿坦克 IsLocallyControlled() == false → 走「从实际位移反推履带线速度」那条路径。
  修复前该路径不存在（只由输入驱动），孤儿/远端坦克的轮子会永远静止。
"""

import unreal

out.clear()


def wheel_yaw(tank, prefix="RoadWheel0"):
    for c in tank.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name().startswith(prefix):
            try:
                return c.get_editor_property("relative_rotation").roll
            except Exception:  # noqa: BLE001
                return None
    return None


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
        orphans = []
        for t in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn):
            if t.get_controller() is None:
                orphans.append(t)

        out.append("服务端世界里无 Controller 的坦克: %d 辆" % len(orphans))
        if not orphans:
            out.append("（先跑 wheel_make_orphan.py 造一个）")
        else:
            for t in orphans:
                loc = t.get_actor_location()
                wy = wheel_yaw(t)
                out.append("  %-14s loc=(%7.0f,%7.0f) wheel0_roll=%s"
                           % (t.get_name(), loc.x, loc.y, ("%.2f" % wy) if wy is not None else "n/a"))

            # 只动第一辆，沿自身前方推 500cm
            t = orphans[0]
            before = wheel_yaw(t)
            fwd = t.get_actor_forward_vector()
            loc = t.get_actor_location()
            newloc = unreal.Vector(loc.x + fwd.x * 500.0, loc.y + fwd.y * 500.0, loc.z)
            t.set_actor_location(newloc, False, False)
            out.append("")
            out.append("已把 %s 沿前方瞬移 500cm（瞬移前 wheel0_roll=%s）"
                       % (t.get_name(), ("%.2f" % before) if before is not None else "n/a"))
            out.append("（瞬移在下一次 Tick 产生位移 → 应触发反推路径把轮子转起来）")

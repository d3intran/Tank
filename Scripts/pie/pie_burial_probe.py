"""M4c-3 取证 · 「半个身子插进土里」诊断探针。

对 PIE 里每一辆车报告：
  A. 位姿 / b_grounded / 真实四底角 gap（负=悬空，正=该角插进地面/穿模）
  B. **穿模深度**：在当前位姿做零长度箱体扫掠（bStartPenetrating?，命中谁），
     再逐级「抬起 5/10/20/40/80/160cm」各测一次，第一个不阻塞的高度 ≈ 埋深
  C. 炮口/炮塔轴心/车体网格包围盒的世界坐标 —— 用于算「不穿模的最大俯角」

用法：`deno run -A Scripts/editor.deno.ts execfile Scripts/pie/pie_burial_probe.py`
"""

import math

import unreal

out.clear()

TRACE = unreal.TraceTypeQuery.ECC_VISIBILITY


def probe_overlap(world, tank, pos, half, rot, label):
    hit = unreal.SystemLibrary.box_trace_single(
        world_context_object=world, start=pos, end=pos, orientation=rot,
        half_size=unreal.Vector(max(1.0, half.x - 0.5), max(1.0, half.y - 0.5), max(1.0, half.z - 0.5)),
        trace_channel=TRACE, trace_complex=False, actors_to_ignore=[tank],
        draw_debug_type=unreal.DrawDebugTrace.NONE, ignore_self=True)
    if hit is None:
        return None
    t = hit.to_tuple()
    try:
        who = t[9].get_actor_label()
    except Exception:  # noqa: BLE001
        who = "?"
    return (who, t[1], t[5].z, t[6].z)   # 命中件 / bStartPenetrating / 命中点 / 法线Z


def report(world, tank, tag):
    loc = tank.get_actor_location()
    rot = tank.get_actor_rotation()
    fwd = tank.get_actor_forward_vector()
    right = tank.get_actor_right_vector()
    up = tank.get_actor_up_vector()
    box = getattr(tank, "collision_box", None)
    half = unreal.Vector(190.0, 87.5, 59.0)
    if box is not None:
        half = box.get_scaled_box_extent()
    out.append("=== %s [%s] loc=(%.1f,%.1f,%.1f) p/y/r=(%.1f,%.1f,%.1f)" % (
        tag, tank.get_name(), loc.x, loc.y, loc.z, rot.pitch, rot.yaw, rot.roll))
    for name in ("b_grounded", "grounded", "bGrounded"):
        try:
            out.append("    grounded=%s" % getattr(tank, name))
            break
        except Exception:  # noqa: BLE001
            continue

    gaps = []
    for ax, ay, nm in ((+half.x, -half.y, "左前"), (+half.x, +half.y, "右前"),
                       (-half.x, -half.y, "左后"), (-half.x, +half.y, "右后")):
        cx = loc.x + fwd.x * ax + right.x * ay + up.x * (-half.z)
        cy = loc.y + fwd.y * ax + right.y * ay + up.y * (-half.z)
        cz = loc.z + fwd.z * ax + right.z * ay + up.z * (-half.z)
        hit = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(cx, cy, cz + 400.0), unreal.Vector(cx, cy, cz - 800.0),
            TRACE, False, [tank], unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            gaps.append("%s=未命中" % nm)
            continue
        gaps.append("%s%+.1f" % (nm, hit.to_tuple()[5].z - cz))
    out.append("    真实角 gap: %s" % " ".join(gaps))

    res = probe_overlap(world, tank, loc, half, rot, "自身")
    if res is None:
        out.append("    B. 当前位姿零长度扫掠：不阻塞（没插进任何东西）")
    else:
        who, pen, hz, nz = res
        out.append("    B. 当前位姿零长度扫掠：命中 <%s> StartPenetrating=%s 命中点Z=%.1f 法线Z=%.2f" % (who, pen, hz, nz))
        depth = None
        for lift in (5, 10, 20, 40, 80, 160, 240):
            r2 = probe_overlap(world, tank, unreal.Vector(loc.x, loc.y, loc.z + lift), half, rot, "lift%d" % lift)
            if r2 is None:
                depth = lift
                break
        out.append("       抬起后不阻塞的最小高度 ≈ %s cm（埋深估计）" % (depth if depth is not None else ">240"))

    gun = getattr(tank, "gun_mesh", None) or getattr(tank, "gun_pivot", None)
    turret = getattr(tank, "turret_pivot", None)
    hull = getattr(tank, "hull_mesh", None)
    if turret is not None:
        p = turret.get_world_location()
        out.append("    炮塔轴心 世界 (%.1f,%.1f,%.1f)" % (p.x, p.y, p.z))
    if getattr(tank, "gun_pivot", None) is not None:
        p = tank.gun_pivot.get_world_location()
        out.append("    火炮轴心 世界 (%.1f,%.1f,%.1f)（车底 Z=%.1f，离地 %.1f）" % (
            p.x, p.y, p.z, loc.z - half.z, p.z - (loc.z - half.z)))
    if gun is not None:
        p = gun.get_world_location()
        out.append("    火炮网格原点 世界 (%.1f,%.1f,%.1f)" % (p.x, p.y, p.z))
    if hull is not None:
        try:
            ob = hull.get_bounds()
            o, e = ob.origin, ob.box_extent
            out.append("    车体网格 包围盒 Z∈[%.1f,%.1f] X∈[%.1f,%.1f]（世界）" % (
                o.z - e.z, o.z + e.z, o.x - e.x, o.x + e.x))
        except Exception:  # noqa: BLE001
            pass


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
    local = None
    for pc in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                local = pc.get_controlled_pawn()
                break
        except Exception:  # noqa: BLE001
            pass
    if local is not None:
        report(server, local, "主机本机车")
    for a in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn):
        if a is not local:
            report(server, a, "服务端他机")

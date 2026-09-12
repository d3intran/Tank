"""6 块坡道的坡底状态：网格最低点 Z（判断埋入/悬空）+ 坡底端外侧地面高度。

建坡脚本把坡底边放在「坡底位置实测地面 - SINK(20)」。
若实测打到了邻居坡面，坡底就会悬空。
"""

import math

import unreal

out.clear()

SINK = 20.0
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

rows = []
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    lbl = a.get_actor_label()
    if not lbl.startswith("Ramp_"):
        continue
    o, e = a.get_actor_bounds(False)
    loc = a.get_actor_location()
    rot = a.get_actor_rotation()
    sc = a.get_actor_scale3d()
    # 局部 X 轴（上坡方向）
    fwd = a.get_actor_forward_vector()
    up = a.get_actor_up_vector()
    half_len = sc.x * 100.0 * 0.5
    # 坡底端中心（网格底面 = loc - 局部Z*厚度/2）
    base_center = unreal.Vector(loc.x - fwd.x * half_len - up.x * (sc.z * 50.0),
                                loc.y - fwd.y * half_len - up.y * (sc.z * 50.0),
                                loc.z - fwd.z * half_len - up.z * (sc.z * 50.0))
    base_surf = unreal.Vector(base_center.x + up.x * (sc.z * 100.0),
                              base_center.y + up.y * (sc.z * 100.0),
                              base_center.z + up.z * (sc.z * 100.0))
    # 坡底端再往外 60cm 探地面
    out_pt = unreal.Vector(base_surf.x - fwd.x * 60.0, base_surf.y - fwd.y * 60.0, 300.0)
    hit = unreal.SystemLibrary.line_trace_single(
        w, out_pt, unreal.Vector(out_pt.x, out_pt.y, -400.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    ground = hit.to_tuple()[5].z if hit is not None else None
    hit_label = hit.to_tuple()[11] if hit is not None else None
    label2 = ""
    try:
        label2 = hit.to_tuple()[11].get_actor_label()
    except Exception:  # noqa: BLE001
        label2 = "?"
    rows.append((lbl, loc, rot, base_surf, ground, label2, o.z - e.z, o.z + e.z))

out.append("=== 6 块坡道坡底状态（SINK=%.0f，网格最低点应 ≈ 坡底-%.0f）" % (SINK, SINK + 78))
for lbl, loc, rot, base_surf, ground, hl, zmin, zmax in sorted(rows):
    float_cm = (base_surf.z - ground) if ground is not None else float("nan")
    out.append("  %-18s yaw=%6.1f pitch=%5.1f | 坡底端面 Z=%7.1f  外侧60cm地面 Z=%7.1f (%s)  悬空=%+7.1f  网格 Z∈[%.1f,%.1f]"
               % (lbl, rot.yaw, rot.pitch, base_surf.z, ground if ground is not None else -999, hl,
                  float_cm, zmin, zmax))

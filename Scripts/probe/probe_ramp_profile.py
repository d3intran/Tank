"""坡道剖面：沿每道坡的上坡中线逐点向下探测，打印「真实可行驶面」的 Z 与所属 actor。

用途：把「坡底是否有坎 / 坡顶是否齐平 / 坡面是不是 40° / 坡底是否悬空」变成数值。
不需要 PIE（读编辑器世界）。
"""

import math

import unreal

out.clear()

STEP = 40.0          # 采样步长（cm）
PAD = 300.0          # 头尾各多探一段，把坡前/坡后的平地也带上


def trace_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 800.0), unreal.Vector(x, y, -200.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, [],
        unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None, "无"
    z = nz = None
    lbl = "?"
    try:
        z = hit.impact_point.z
        nz = hit.impact_normal.z
    except Exception:  # noqa: BLE001
        t = hit.to_tuple()
        if len(t) > 5 and hasattr(t[5], "z"):
            z, nz = t[5].z, t[6].z
    try:
        lbl = hit.to_tuple()[11].get_actor_label()
    except Exception:  # noqa: BLE001
        pass
    return z, "%s(法线%.2f)" % (lbl, nz if nz is not None else -9)


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

ramps = sorted([a for a in unreal.EditorLevelLibrary.get_all_level_actors()
                if a.get_actor_label().startswith("Ramp_")],
               key=lambda a: a.get_actor_label())

for r in ramps:
    lbl = r.get_actor_label()
    loc = r.get_actor_location()
    rot = r.get_actor_rotation()
    sc = r.get_actor_scale3d()
    t = r.get_actor_forward_vector()     # 上坡方向（本地 +X）
    up = r.get_actor_up_vector()         # 坡面法线
    slope_len = sc.x * 100.0
    surf_c = unreal.Vector(loc.x + up.x * sc.z * 50.0,
                           loc.y + up.y * sc.z * 50.0,
                           loc.z + up.z * sc.z * 50.0)
    out.append("=== %s @ (%.0f,%.0f,%.0f) 坡度 %.1f° 斜面 %.0f ===" % (lbl, loc.x, loc.y, loc.z, rot.pitch, slope_len))
    out.append("    坡面中线：沿上坡方向逐点向下探测（u=0 是坡心，负=坡底侧，正=坡顶侧）")
    u = -slope_len * 0.5 - PAD
    prev = None
    while u <= slope_len * 0.5 + PAD:
        px = surf_c.x + t.x * u
        py = surf_c.y + t.y * u
        z, info = trace_z(world, px, py)
        if z is None:
            out.append("    u=%+7.0f  (%.0f,%.0f)  Z=  ----  %s" % (u, px, py, info))
        else:
            slope_txt = ""
            if prev is not None:
                dz = z - prev[1]
                slope_txt = "  ΔZ=%+6.1f(%.0f°)" % (dz, math.degrees(math.atan2(dz, STEP)))
            out.append("    u=%+7.0f  (%.0f,%.0f)  Z=%7.1f  %s%s" % (u, px, py, z, info, slope_txt))
            prev = (u, z, px, py)
        u += STEP
    out.append("")

"""坡-掩体接缝纵剖：沿每条坡的中轴线从坡外 100cm 走到坡顶内 150cm，
每 25cm 向下探一次地面，记录命中件与高度 —— 用于确认「坡面正好交在掩体顶面棱上」。

判据：坡面终点高度 == 掩体顶面高度（460），且没有高于/低于 460 的台阶跳变。
"""

import unreal

out.clear()

w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

rows = []
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    lbl = a.get_actor_label()
    if not lbl.startswith("Ramp_"):
        continue
    loc = a.get_actor_location()
    sc = a.get_actor_scale3d()
    fwd = a.get_actor_forward_vector()
    up = a.get_actor_up_vector()
    half = sc.x * 50.0
    thick_half = sc.z * 50.0
    base = unreal.Vector(loc.x - fwd.x * half + up.x * thick_half,
                         loc.y - fwd.y * half + up.y * thick_half,
                         loc.z - fwd.z * half + up.z * thick_half)
    rows.append((lbl, base, fwd))

for lbl, base, fwd in sorted(rows):
    out.append("=== %s ===" % lbl)
    prev = None
    for step in range(-4, 45):
        d = step * 25.0
        px = base.x + fwd.x * d
        py = base.y + fwd.y * d
        hit = unreal.SystemLibrary.line_trace_single(
            w, unreal.Vector(px, py, 800.0), unreal.Vector(px, py, -300.0),
            unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            out.append("  d=%6.1f  未命中" % d)
            continue
        t = hit.to_tuple()
        z = t[5].z
        try:
            who = t[9].get_actor_label()   # [9]=命中 Actor（[5]=ImpactPoint，见 probe_hit_tuple.py）
        except Exception:  # noqa: BLE001
            who = "?"
        jump = "" if prev is None else "  Δ=%+.1f" % (z - prev)
        out.append("  d=%6.1f  Z=%7.1f  <%s>%s" % (d, z, who, jump))
        prev = z
    out.append("")

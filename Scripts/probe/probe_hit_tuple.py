"""FHitResult.to_tuple() 索引表 + 掩体/坡顶高度实测（一次性的结构复验）。

要点：pitch 30° 的坡面最高到 460，探针起点必须高于此，否则会"命中自己的起点"（Time=0，Location=起点）。
"""

import unreal

out.clear()

w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def dump(label, x, y, start_z=800.0):
    hit = unreal.SystemLibrary.line_trace_single(
        w, unreal.Vector(x, y, start_z), unreal.Vector(x, y, -300.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        out.append("%s: 未命中" % label)
        return
    t = hit.to_tuple()
    out.append("--- %s @ (%.0f,%.0f) 起点Z=%.0f ---" % (label, x, y, start_z))
    for i, v in enumerate(t):
        s = str(v)
        if len(s) > 60:
            s = s[:60] + "…"
        out.append("   [%2d] %-28s %s" % (i, type(v).__name__, s))


# 掩体正中上方（看掩体顶面）
dump("Cover_SE 正中", 1600.0, 3000.0)
# SE 坡中段（看坡面）
dump("Ramp_SE_X 中段", 2400.0, 3000.0)
# 交界的掩体表面外 10cm（看接缝）
dump("SE 坡顶(掩体表面外 10cm)", 2010.0, 3000.0)

# 掩体几何：bounds 与 scale
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    lbl = a.get_actor_label()
    if lbl.startswith("Cover_") or lbl.startswith("Ramp_"):
        loc = a.get_actor_location()
        sc = a.get_actor_scale3d()
        o, e = a.get_actor_bounds(False)
        out.append("%-18s loc Z=%7.1f  scale=(%.0f,%.0f,%.0f)  bounds Z∈[%.1f,%.1f]"
                   % (lbl, loc.z, sc.x, sc.y, sc.z, o.z - e.z, o.z + e.z))

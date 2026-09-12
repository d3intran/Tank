"""复验 B5：Ramp_SW_Y 坡底悬空 + 与 Ramp_Center_L_Y 坡底对撞。

沿 x=-1600 从 Y=3800 扫到 3950（10cm 步进），竖直向下探测（z=300 → -100）。
预期对比：坡面（normal≈0.77, 40°）在哪结束、路面（normal=1, z≈2.2）从哪开始。
"""

import unreal

out.clear()

X = -1600.0
samples = []


def trace(world, start, end):
    hit = unreal.SystemLibrary.line_trace_single(
        world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    label = t[9] or t[11] or t[10] or t[12] or "?"
    return t[5].z, t[6].z, str(label).split(":")[-1].split("'")[0]


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
    y = 3800.0
    while y <= 3950.5:
        r = trace(server, unreal.Vector(X, y, 300.0), unreal.Vector(X, y, -100.0))
        if r is None:
            samples.append("Y=%.0f  无命中" % y)
        else:
            samples.append("Y=%.0f  hitZ=%7.2f  normalZ=%.2f  %s" % (y, r[0], r[1], r[2]))
        y += 10.0
    out.append("x=%.0f 纵剖（自上而下）:" % X)
    for line in samples:
        out.append("  " + line)

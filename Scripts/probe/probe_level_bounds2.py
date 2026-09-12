"""关卡边界/地面实测现状：Floor / road_hd / road_hd2 / Bound_* 的实际 AABB，推算可行驶范围。

背景：M4 日志记录 Bound_East 中心 X=3069、高 100；实测却是 X∈[3449,3649]、高 241。
需要把三个边界台阶与地面/路面的真实范围重新量一遍（PIE server 世界）。
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    w = max(unreal.EditorLevelLibrary.get_pie_worlds(False), key=lambda x: len(
        unreal.GameplayStatics.get_all_actors_of_class(x, unreal.PlayerController)))
    out.append("世界=%s（server）" % w.get_name())
else:
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

TARGETS = ("Floor", "road_hd", "road_hd2", "SM_Template_Map_Floor")
acts = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor)

out.append("")
out.append("=== 地面/路面/台阶 AABB")
for a in acts:
    lb = a.get_actor_label()
    if not (lb in TARGETS or lb.startswith("Bound_")):
        continue
    o, e = a.get_actor_bounds(False)
    sc = a.get_actor_scale3d()
    out.append("  %-20s 中心(%7.0f,%7.0f,%6.1f) 半尺寸(%6.0f,%6.0f,%6.1f)  X∈[%.0f,%.0f] Y∈[%.0f,%.0f] Z∈[%.0f,%.0f] scale=(%.2f,%.2f,%.2f)"
               % (lb, o.x, o.y, o.z, e.x, e.y, e.z,
                  o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z,
                  sc.x, sc.y, sc.z))

out.append("")
out.append("=== 边界台阶实测挡车点（每侧取内表面上一点做水平射线）")
for lbl, x0, y0, x1, y1 in (("东", 3000.0, 3000.0, 3800.0, 3000.0),
                            ("西", -3000.0, 3000.0, -3800.0, 3000.0),
                            ("北", 0.0, 7000.0, 0.0, 8000.0)):
    hit = unreal.SystemLibrary.line_trace_single(
        w, unreal.Vector(x0, y0, 60.0), unreal.Vector(x1, y1, 60.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        out.append("  %s：Z60 水平射线无命中（%s→%s）" % (lbl, (x0, y0), (x1, y1)))
    else:
        t = hit.to_tuple()
        hit_lbl = "?"
        for i in (9, 11, 10, 12):
            if i < len(t) and hasattr(t[i], "get_actor_label"):
                hit_lbl = t[i].get_actor_label()
                break
        out.append("  %s：命中(%.0f,%.0f,%.0f) <%s>" % (lbl, t[4].x, t[4].y, t[4].z, hit_lbl))

out.append("")
out.append("=== 台架外侧是否还有地面（在台阶外 100cm 处向下探）")
for lbl, x, y in (("东外", 3750.0, 3000.0), ("西外", -3750.0, 3000.0),
                  ("北外", 0.0, 7700.0), ("东内", 3350.0, 3000.0),
                  ("东北角外", 3750.0, 7700.0)):
    hit = unreal.SystemLibrary.line_trace_single(
        w, unreal.Vector(x, y, 600.0), unreal.Vector(x, y, -600.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        out.append("  %s (%.0f,%.0f)：无命中 → 悬空/无地面" % (lbl, x, y))
    else:
        t = hit.to_tuple()
        hit_lbl = "?"
        for i in (9, 11, 10, 12):
            if i < len(t) and hasattr(t[i], "get_actor_label"):
                hit_lbl = t[i].get_actor_label()
                break
        out.append("  %s (%.0f,%.0f)：命中 Z=%.1f <%s>" % (lbl, x, y, t[5].z, hit_lbl))

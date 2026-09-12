"""Bound_East 详细体检：视觉包围盒 vs 碰撞包围盒 / 碰撞 profile / 多射线交叉。

疑点：竖直射线 @(3069,3000) 穿过台阶命中 road_hd(2.2)，水平射线 @Z60 却被 Bound_East 挡住。
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
in_pie = les.is_in_play_in_editor()
if in_pie:
    w = max(unreal.EditorLevelLibrary.get_pie_worlds(False), key=lambda x: len(
        unreal.GameplayStatics.get_all_actors_of_class(x, unreal.PlayerController)))
else:
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
    if a.get_actor_label() != "Bound_East":
        continue
    loc = a.get_actor_location()
    rot = a.get_actor_rotation()
    sc = a.get_actor_scale3d()
    out.append("Bound_East loc=(%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f) scale=(%.2f,%.2f,%.2f)"
               % (loc.x, loc.y, loc.z, rot.pitch, rot.yaw, rot.roll, sc.x, sc.y, sc.z))
    for flag in (False, True):
        try:
            o, e = a.get_actor_bounds(flag)
            out.append("  bounds(only_colliding=%s): 中心(%.1f,%.1f,%.1f) 半尺寸(%.1f,%.1f,%.1f) → X∈[%.0f,%.0f] Y∈[%.0f,%.0f] Z∈[%.0f,%.0f]"
                       % (flag, o.x, o.y, o.z, e.x, e.y, e.z,
                          o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z))
        except Exception as exc:  # noqa: BLE001
            out.append("  bounds(%s) 失败: %s" % (flag, exc))
    try:
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        if smc is not None:
            out.append("  mesh=%s mobility=%s collision=%s profile=%s"
                       % (smc.get_editor_property("static_mesh"), smc.get_editor_property("mobility"),
                          smc.get_collision_enabled(), smc.get_collision_profile_name()))
            for ch_name, ch in (("Visibility", unreal.TraceTypeQuery.ECC_VISIBILITY),
                                ("Pawn", unreal.TraceTypeQuery.ECC_PAWN),
                                ("Camera", unreal.TraceTypeQuery.ECC_CAMERA)):
                try:
                    resp = smc.get_collision_response_to_channel(ch)
                    out.append("    对 %s: %s" % (ch_name, resp))
                except Exception as exc:  # noqa: BLE001
                    out.append("    对 %s 查询失败: %s" % (ch_name, exc))
    except Exception as exc:  # noqa: BLE001
        out.append("  读组件失败: %s" % exc)


def tr(x0, y0, z0, x1, y1, z1, ch, tag):
    hit = unreal.SystemLibrary.line_trace_single(
        w, unreal.Vector(x0, y0, z0), unreal.Vector(x1, y1, z1),
        ch, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        out.append("  %s: 无命中" % tag)
        return
    t = hit.to_tuple()
    lbl = "?"
    for i in (9, 11, 10, 12):
        if i < len(t) and hasattr(t[i], "get_actor_label"):
            lbl = t[i].get_actor_label()
            break
    out.append("  %s: 命中(%.0f,%.0f,%.0f) Z=%.1f <%s>" % (tag, t[4].x, t[4].y, t[4].z, t[5].z, lbl))


out.append("")
out.append("射线交叉（VISIBILITY）：")
tr(3069, 3000, 400, 3069, 3000, -400, unreal.TraceTypeQuery.ECC_VISIBILITY, "竖直@(3069,3000)")
tr(3069, 4426, 400, 3069, 4426, -400, unreal.TraceTypeQuery.ECC_VISIBILITY, "竖直@(3069,4426)")
tr(3069, 2000, 400, 3069, 2000, -400, unreal.TraceTypeQuery.ECC_VISIBILITY, "竖直@(3069,2000)")
tr(2870, 3000, 60, 3300, 3000, 60, unreal.TraceTypeQuery.ECC_VISIBILITY, "水平@Z60 (2870→3300)")
tr(2870, 3000, 110, 3300, 3000, 110, unreal.TraceTypeQuery.ECC_VISIBILITY, "水平@Z110 (2870→3300)")
try:
    tr(2870, 3000, 60, 3300, 3000, 60, unreal.TraceTypeQuery.ECC_PAWN, "水平@Z60 PAWN通道")
except Exception as exc:  # noqa: BLE001
    out.append("  PAWN 通道射线失败: %s" % exc)
tr(2870, 4426, 60, 3300, 4426, 60, unreal.TraceTypeQuery.ECC_VISIBILITY, "水平@Z60 Y=4426")

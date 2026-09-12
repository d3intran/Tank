"""量场地边界：地面/城门/公路的包围盒，用于安全地设计出生环与掩体位置。"""

import unreal

out.clear()

ws = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.EditorLevelLibrary.get_all_level_actors()


def bounds_of(a):
    o, e = a.get_actor_bounds(False)
    return o, e


out.append("=== 关键几何的包围盒（origin / extent）===")
for a in actors:
    if not isinstance(a, unreal.StaticMeshActor):
        continue
    smc = a.get_component_by_class(unreal.StaticMeshComponent)
    mesh = smc.get_editor_property("static_mesh") if smc else None
    name = mesh.get_name() if mesh else "<none>"
    if name == "SM_SkySphere":
        continue
    o, e = bounds_of(a)
    out.append("  %-30s %-34s origin=(%8.0f,%8.0f,%8.0f) extent=(%7.0f,%7.0f,%7.0f)"
               % (a.get_name(), name, o.x, o.y, o.z, e.x, e.y, e.z))

# 地面单独给一个"可用范围"结论
floor = None
for a in actors:
    if isinstance(a, unreal.StaticMeshActor):
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        mesh = smc.get_editor_property("static_mesh") if smc else None
        if mesh and mesh.get_name() == "SM_Template_Map_Floor":
            floor = a
            break

out.append("")
if floor:
    o, e = bounds_of(floor)
    out.append("=== 地面可用范围（以地面包围盒为准）===")
    out.append("  X: %.0f ~ %.0f   Y: %.0f ~ %.0f   顶面 Z ≈ %.0f"
               % (o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z + e.z))
    out.append("  尺寸: %.0f x %.0f" % (2 * e.x, 2 * e.y))
else:
    out.append("没找到 SM_Template_Map_Floor")

# 现有 3 个出生点里最小的那个半径，作为"别放太远"的参考
starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
out.append("")
out.append("=== 现有出生点（%d 个）===" % len(starts))
for s in starts:
    l = s.get_actor_location()
    out.append("  %-16s (%8.0f,%8.0f,%8.0f)" % (s.get_name(), l.x, l.y, l.z))

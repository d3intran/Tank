"""掩体/坡布局 dump：列出 Cover_/Ramp_ 的 AABB 与朝向，用于选目验机位（PIE 服务器世界）。"""

import unreal

out.clear()

world = None
for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
    pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
    if len(pcs) > 1:
        world = w
        break
if world is None:
    out.append("PIE 未运行")
else:
    acts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor)
    for a in sorted(acts, key=lambda x: x.get_actor_label()):
        lbl = a.get_actor_label()
        if not (lbl.startswith("Cover_") or lbl.startswith("Ramp_")):
            continue
        o, e = a.get_actor_bounds(False)
        rot = a.get_actor_rotation()
        out.append("%-18s X %7.0f~%7.0f  Y %7.0f~%7.0f  Z %6.1f~%6.1f  p=%.0f y=%.0f r=%.0f"
                   % (lbl, o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y,
                      o.z - e.z, o.z + e.z, rot.pitch, rot.yaw, rot.roll))

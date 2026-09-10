import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = ues.get_editor_world()

gate = None
proxy = None
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    label = a.get_actor_label()
    if label == "CityGate_Wall":
        gate = a
    elif label == "SmokeWall_Proxy":
        proxy = a

if not gate or not proxy:
    out.append("ERROR: gate=%s proxy=%s" % (bool(gate), bool(proxy)))
else:
    origin, extent = gate.get_actor_bounds(False)
    mesh = proxy.get_components_by_class(unreal.StaticMeshComponent)[0]
    # set_world_scale3d 是绝对世界缩放：目标尺寸 / 100（基础立方 100cm）
    mesh.set_world_scale3d(unreal.Vector(
        (extent.x * 2.0) / 100.0, 200.0 / 100.0, (extent.z * 2.0) / 100.0))
    proxy.set_actor_location(unreal.Vector(origin.x, origin.y + extent.y + 100.0, extent.z), False, False)
    cm = proxy.get_components_by_class(unreal.ClimbManager)
    if cm:
        cm[0].set_editor_property("pile_half_width", 1500.0)
    b = proxy.get_actor_bounds(False)
    out.append("proxy bounds extent=(%.0f,%.0f,%.0f) at (%.0f,%.0f,%.0f) halfwidth=1500" % (
        b[1].x, b[1].y, b[1].z, b[0].x, b[0].y, b[0].z))
    out.append("level saved: %s" % unreal.EditorLevelLibrary.save_current_level())

import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

gate = None
proxy = None
spawner = None
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    label = a.get_actor_label()
    if label == "CityGate_Wall":
        gate = a
    elif label == "SmokeWall_Proxy":
        proxy = a
    elif label == "SmokeSpawner_P1":
        spawner = a

if not gate or not proxy:
    out.append("ERROR: gate=%s proxy=%s" % (bool(gate), bool(proxy)))
else:
    origin, extent = gate.get_actor_bounds(False)
    # 丧尸从北面旷野来（spawner y≈7800）→ 代理贴城门北面
    ny = origin.y + extent.y + 100.0
    loc = unreal.Vector(origin.x, ny, extent.z)
    proxy.set_actor_location(loc, False, False)
    mesh = proxy.get_components_by_class(unreal.StaticMeshComponent)[0]
    mesh.set_world_scale3d(unreal.Vector((extent.x * 2.0) / 2000.0, 1.0, (extent.z * 2.0) / 774.0))
    out.append("proxy moved to north face (%.0f,%.0f,%.0f), height=%.0f" % (
        loc.x, loc.y, loc.z, extent.z * 2.0))
    out.append("level saved: %s" % unreal.EditorLevelLibrary.save_current_level())

import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

gate = None
smoke_wall = None
spawner = None
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    label = a.get_actor_label()
    if label == "CityGate_Wall":
        gate = a
    elif label == "SmokeWall_P1":
        smoke_wall = a
    elif label == "SmokeSpawner_P1":
        spawner = a

if not gate:
    out.append("ERROR: CityGate_Wall not found")
else:
    origin, extent = gate.get_actor_bounds(False)
    out.append("gate bounds: origin=(%.0f,%.0f,%.0f) extent=(%.0f,%.0f,%.0f)" % (
        origin.x, origin.y, origin.z, extent.x, extent.y, extent.z))

    # 朝向标定：丧尸从南边来——代理墙紧贴城门南侧放置
    # 需要哪根轴是"厚度轴"：比较 x/y extent
    thin_is_y = extent.y < extent.x
    out.append("thin axis: %s" % ("Y" if thin_is_y else "X"))

    # 清掉旧的代理（幂等）
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if a.get_actor_label() == "SmokeWall_Proxy":
            eas.destroy_actor(a)

    # 代理墙：几何对齐城门南面（长度/高用城门包围盒，厚度 200 固定）
    wall_cls = unreal.load_class(None, "/Script/Tank.DefWall")
    if thin_is_y:
        loc = unreal.Vector(origin.x, origin.y - extent.y - 100.0, extent.z / 2.0)
        rot = unreal.Rotator(0.0, 0.0, 0.0)
        sx = (extent.x * 2.0) / 2000.0
        sy = 200.0 / 200.0
    else:
        loc = unreal.Vector(origin.x - extent.x - 100.0, origin.y, extent.z / 2.0)
        rot = unreal.Rotator(0.0, 90.0, 0.0)
        sx = (extent.y * 2.0) / 2000.0
        sy = 200.0 / 200.0
    proxy = eas.spawn_actor_from_class(wall_cls, loc, rot)
    proxy.set_actor_label("SmokeWall_Proxy")
    mesh = proxy.get_components_by_class(unreal.StaticMeshComponent)[0]
    mesh.set_world_scale3d(unreal.Vector(sx, sy, extent.z / 774.0))
    mesh.set_collision_profile_name("NoCollision")
    proxy.set_actor_hidden_in_game(True)
    out.append("proxy spawned at (%.0f,%.0f,%.0f) scale=(%.1f,%.1f,%.2f) hidden" % (
        loc.x, loc.y, loc.z, sx, sy, extent.z / 774.0))

    if spawner:
        spawner.set_editor_property("target_wall", proxy)
        out.append("spawner retargeted to proxy (at real gate)")
    if smoke_wall:
        eas.destroy_actor(smoke_wall)
        out.append("old SmokeWall_P1 destroyed")

    out.append("level saved: %s" % unreal.EditorLevelLibrary.save_current_level())

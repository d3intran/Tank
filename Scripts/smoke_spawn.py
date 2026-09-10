import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 幂等：清掉上次冒烟件
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    if a.get_actor_label().startswith(("SmokeWall", "SmokeSpawner")):
        eas.destroy_actor(a)

# 1. 灰盒墙：路面尽头，基座落在地面 Z≈5 上（墙高 774，actor 原点在半高 387）
wall = eas.spawn_actor_from_class(
    unreal.load_class(None, "/Script/Tank.DefWall"),
    unreal.Vector(-90.0, 9000.0, 392.0), unreal.Rotator(0.0, 0.0, 0.0))
wall.set_actor_label("SmokeWall_P1")
out.append("wall spawned: %s" % wall.get_actor_label())

# 2. 刷怪器：坦克前方 28m，20 只间隔 1s；Z=95 让胶囊底贴地（90 半高 + 5 地面）
spawner = eas.spawn_actor_from_class(
    unreal.load_class(None, "/Script/Tank.ZombieSpawner"),
    unreal.Vector(-90.0, 7500.0, 95.0), unreal.Rotator(0.0, 0.0, 0.0))
spawner.set_actor_label("SmokeSpawner_P1")
spawner.set_editor_property("target_wall", wall)
spawner.set_editor_property("total_to_spawn", 20)
spawner.set_editor_property("spawn_interval", 1.0)
out.append("spawner spawned: target=%s total=%d" % (
    spawner.get_editor_property("target_wall").get_actor_label(),
    spawner.get_editor_property("total_to_spawn")))

# 3. 关卡 GameMode 指向 ADefGameMode（提供 HUD 与胜负判定）
ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", unreal.load_class(None, "/Script/Tank.DefGameMode"))
out.append("game mode set: %s" % ws.get_editor_property("default_game_mode").get_name())

# 4. 保存关卡（P1 场景配置持久化）
saved = unreal.EditorLevelLibrary.save_current_level()
out.append("level saved: %s" % saved)

import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    if a.get_actor_label() == "SmokeSpawner_P1":
        a.set_actor_location(unreal.Vector(-90.0, 7800.0, 95.0), False, False)
        out.append("spawner moved to y=7800")
        break
out.append("level saved: %s" % unreal.EditorLevelLibrary.save_current_level())

import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    if a.get_actor_label() == "SmokeSpawner_P1":
        a.set_editor_property("total_to_spawn", 100)
        a.set_editor_property("spawn_interval", 0.2)
        out.append("density: total=%d interval=%.1f" % (
            a.get_editor_property("total_to_spawn"),
            a.get_editor_property("spawn_interval")))
        break
out.append("level saved: %s" % unreal.EditorLevelLibrary.save_current_level())

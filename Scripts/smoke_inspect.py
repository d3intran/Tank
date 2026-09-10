import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
for a in actors:
    loc = a.get_actor_location()
    origin, extent = a.get_actor_bounds(False)
    out.append("%-28s [%-24s] @ (%7.0f,%7.0f,%5.0f) ext=(%.0f,%.0f,%.0f)" % (
        a.get_name(), a.get_class().get_name(), loc.x, loc.y, loc.z,
        extent.x, extent.y, extent.z))

import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = ues.get_editor_world()
out.append("world: %s (%s)" % (world.get_name(), world.get_path_name()))
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
out.append("total actors: %d" % len(actors))
labels = sorted(set(a.get_actor_label() for a in actors))
out.append("labels: %s" % ", ".join(labels[:60]))

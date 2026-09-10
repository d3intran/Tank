import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = ues.get_game_world()
if not world:
    out.append("no game world")
else:
    zombies = []
    wall_hp = None
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        cn = a.get_class().get_name()
        if cn == "AZombie":
            zombies.append(a)
        elif cn == "StaticMeshActor" and a.get_actor_label() == "SmokeWall_Proxy":
            wh = a.get_components_by_class(unreal.WallHealthComponent)
            if wh:
                wall_hp = wh[0].get_editor_property("current_health")
    out.append("zombies=%d wall_hp=%s" % (len(zombies), wall_hp))
    from collections import Counter
    states = Counter()
    for z in zombies:
        states[str(z.get_editor_property("state"))] += 1
    out.append("states: %s" % dict(states))
    for z in zombies[:5]:
        loc = z.get_actor_location()
        out.append("%s %s @ (%.0f,%.0f,%.0f)" % (
            z.get_name(), z.get_editor_property("state"), loc.x, loc.y, loc.z))

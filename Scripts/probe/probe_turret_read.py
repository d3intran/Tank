"""列出坦克的组件与炮塔相关可读属性，用于确认「炮塔朝向」的 Python 读法。"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    server = None
    best = 0
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
        n = len(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController))
        if n > best:
            best, server = n, w

    tanks = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn)
    if not tanks:
        out.append("服务器世界没有 TankPawn")
    else:
        t = tanks[0]
        out.append("样本 = %s" % t.get_name())

        out.append("")
        out.append("含 pivot/turret/gun/mesh 的属性名:")
        out.append("  %s" % ", ".join(
            m for m in dir(t) if not m.startswith("_")
            and any(k in m.lower() for k in ("pivot", "turret", "gun", "mesh", "hull", "track"))))

        out.append("")
        out.append("get_components_by_class(SceneComponent) → 名字 / 类:")
        try:
            comps = t.get_components_by_class(unreal.SceneComponent)
            out.append("  共 %d 个" % len(comps))
            for c in comps:
                out.append("    %-28s %s" % (c.get_name(), c.get_class().get_name()))
        except Exception as exc:  # noqa: BLE001
            out.append("  异常 %s" % exc)

        out.append("")
        out.append("逐个属性取值试读:")
        for attr in ("turret_pivot", "gun_pivot"):
            comp = getattr(t, attr, None)
            out.append("  %-14s -> %s" % (attr, comp))
            if comp is not None:
                try:
                    out.append("      rel_rot = %s" % comp.get_relative_rotation())
                except Exception as exc:  # noqa: BLE001
                    out.append("      rel_rot 异常 %s" % exc)

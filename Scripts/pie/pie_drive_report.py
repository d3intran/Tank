"""报告 PIE 各界里坦克的位姿与炮塔朝向（用于验证客户端移动 / 炮塔同步）。

为什么读组件旋转而不是炮塔角度：`ATankPawn::GetCurrentTurretYaw()` 是 FORCEINLINE、未反射，
Python 调不到；但炮塔/火炮是两个 USceneComponent（TurretPivot / GunPivot），
它们的相对旋转就是当前炮塔 yaw 与火炮 pitch，属于公开可读状态。

跨世界对齐只能靠名字 + 归属判断，因为每个 PIE 世界各自给 Actor 命名，
「远端看到的这辆车」与「本机自己的这辆车」在日志/列表里是不同对象。
约定：结果收集进 out 列表；严禁对 out 重新赋值，开头清空用 out.clear()。
"""

import unreal

out.clear()


def turret_of(pawn):
    """返回 (炮塔相对 yaw, 火炮相对 pitch)；组件缺失给 None。

    优先读暴露的组件属性（TurretPivot/GunPivot 是 UPROPERTY(VisibleAnywhere, BlueprintReadOnly)，
    Python 可见）；属性取不到再退回按名字扫组件。
    """
    def rel_rot(comp, axis):
        if comp is None:
            return None
        try:
            return getattr(comp.get_relative_rotation(), axis)
        except Exception:  # noqa: BLE001
            return None

    yaw = rel_rot(getattr(pawn, "turret_pivot", None), "yaw")
    pitch = rel_rot(getattr(pawn, "gun_pivot", None), "pitch")
    if yaw is not None or pitch is not None:
        return yaw, pitch

    try:
        comps = pawn.get_components_by_class(unreal.SceneComponent)
    except Exception:  # noqa: BLE001
        return yaw, pitch
    for c in comps:
        name = c.get_name()
        try:
            rot = c.get_relative_rotation()
        except Exception:  # noqa: BLE001
            continue
        if "TurretPivot" in name:
            yaw = rot.yaw
        elif "GunPivot" in name:
            pitch = rot.pitch
    return yaw, pitch


def local_pc_names(world):
    names = set()
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller():
                names.add(pc.get_name())
        except Exception:  # noqa: BLE001
            pass
    return names


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    counts = {}
    for w in worlds:
        counts[w] = len(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController))
    server = max(counts, key=lambda k: counts[k]) if counts else None

    out.append("PIE 世界数 = %d；服务端 = %s"
               % (len(worlds), server.get_name() if server else "未识别"))
    out.append("")

    for w in worlds:
        is_server = (w is server)
        out.append("--- %s%s" % (w.get_name(), "  [服务端]" if is_server else "  [客户端]"))
        localset = local_pc_names(w)
        tanks = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.TankPawn)
        for t in sorted(tanks, key=lambda a: a.get_name()):
            c = t.get_controller()
            cname = c.get_name() if c else "NONE"
            kind = "本机" if cname in localset else ("远端" if c else "无主")
            loc = t.get_actor_location()
            rot = t.get_actor_rotation()
            tyaw, gpitch = turret_of(t)
            try:
                tick = "Tick开" if t.is_actor_tick_enabled() else "Tick关"
            except Exception:  # noqa: BLE001
                tick = "Tick?"
            out.append("  %-12s %s ctrl=%-18s %s loc=(%8.0f,%8.0f) 车体yaw=%6.1f 炮塔yaw=%s 火炮pitch=%s"
                       % (t.get_name(), kind, cname, tick, loc.x, loc.y, rot.yaw,
                          "%.1f" % tyaw if tyaw is not None else "n/a",
                          "%.1f" % gpitch if gpitch is not None else "n/a"))
        out.append("")

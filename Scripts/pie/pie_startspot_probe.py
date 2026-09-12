"""判别实验：`ShouldSpawnAtStartSpot` 覆盖是否真的生效（Bug 2）。

原理：
  - 引擎默认实现在 `Player->StartSpot != nullptr` 时返回 true → FindPlayerStart 直接返回
    「首次出生点」，ChoosePlayerStart 永远不被调用。
  - 我们把另外两辆车瞬移到主机首次出生点 (-1800,5000) 附近堆成一堆，然后打死主机的车。
    * 若覆盖生效（返回 false）→ 走 ChoosePlayerStart「离存活坦克最远」→ 主机**不会**重生在
      (-1800,5000)，而是跑到远处去。
    * 若覆盖没生效 → 主机固定回自己的 StartSpot (-1800,5000)，正好落在车堆里。
  两种结果的落点差异非常明显，可判定。
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未在运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = None
    for w in worlds:
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            server = w
            break

    if server is None:
        out.append("没找到服务端世界")
    else:
        pcs = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController)
        tanks = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn)

        out.append("--- 各 PC 的 StartSpot（首次出生点）---")
        for pc in pcs:
            try:
                ss = pc.get_editor_property("start_spot")
                out.append("  %s start_spot=%s" % (pc.get_name(),
                                                   ss.get_name() if ss else "NONE"))
            except Exception as exc:  # noqa: BLE001
                out.append("  %s start_spot 读取失败: %s" % (pc.get_name(), exc))

        host_tank = None
        others = []
        for t in tanks:
            c = t.get_controller()
            if c is not None and c.get_name().endswith("Controller_0"):
                host_tank = t
            else:
                others.append(t)

        out.append("")
        out.append("--- 把非主机坦克瞬移到主机出生点附近，制造车堆 ---")
        cluster = unreal.Vector(-1800.0, 5000.0, 123.0)
        for i, t in enumerate(others):
            tgt = unreal.Vector(cluster.x + 250.0 * (i + 1), cluster.y, cluster.z)
            t.set_actor_location(tgt, False, False)
            out.append("  %s -> (%.0f, %.0f)" % (t.get_name(), tgt.x, tgt.y))

        out.append("")
        if host_tank is None:
            out.append("没找到主机的坦克")
        else:
            loc = host_tank.get_actor_location()
            out.append("主机坦克 %s 当前位置 (%.0f, %.0f)" % (host_tank.get_name(), loc.x, loc.y))
            out.append("即将击杀主机坦克，观察重生落点是否仍是 (-1800, 5000)")
            unreal.GameplayStatics.apply_damage(host_tank, 9999.0,
                                                host_tank.get_controller(), host_tank, None)
            out.append("已击杀，等待巡检补发…")

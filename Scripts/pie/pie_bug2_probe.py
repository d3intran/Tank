"""Bug 2 判别实验（决定性版）。

上一版失败原因：客户端坦克是客户端权威的，服务器端瞬移会被客户端的
`ServerSyncTransform`（50Hz 上报）立刻覆盖回去。
但**主机自己的坦克是服务端本地控制的**，服务器端瞬移会保留（无客户端来覆盖）。

实验设计：
  1. 把主机的坦克瞬移到客户端1首次出生点 (1800,5000) 旁边（+80cm）。
  2. 打死客户端1的坦克。
  3. 观察客户端1的新车落在哪：
     * 若 ShouldSpawnAtStartSpot 覆盖**生效**（返回 false）→ 走 ChoosePlayerStart
       「离存活坦克最远」→ 绝不会落在 (1800,5000)，会跑到远处。
     * 若覆盖**没生效** → FindPlayerStart 直接返回客户端1的 StartSpot (1800,5000)
       → 正好叠在主机坦克旁边（相距约 80cm）。
  落点差异极大，可判定。
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
        tanks = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn)
        host_tank = None
        client1_tank = None
        for t in tanks:
            c = t.get_controller()
            if c is None:
                continue
            if c.get_name().endswith("Controller_0"):
                host_tank = t
            elif c.get_name().endswith("Controller_1"):
                client1_tank = t

        if host_tank is None or client1_tank is None:
            out.append("没找到主机或客户端1的坦克 (host=%s c1=%s)"
                       % (host_tank, client1_tank))
        else:
            # 把主机坦克挪到客户端1出生点旁边 80cm（服务端权威，不会被覆盖）
            target = unreal.Vector(1880.0, 5000.0, 123.0)
            host_tank.set_actor_location(target, False, False)
            out.append("主机坦克 %s 已瞬移到 (1880, 5000) —— 紧邻客户端1的出生点 (1800,5000)"
                       % host_tank.get_name())
            out.append("客户端1坦克 %s 当前位置 %s"
                       % (client1_tank.get_name(), client1_tank.get_actor_location()))
            out.append("")
            out.append("击杀客户端1的坦克，观察新车落点：")
            out.append("  - 落在 (1800,5000) 附近（与主机车重叠）=> Bug2 修复**未生效**")
            out.append("  - 落在别处（远离主机车）        => Bug2 修复**生效**")
            unreal.GameplayStatics.apply_damage(client1_tank, 9999.0,
                                                client1_tank.get_controller(), client1_tank, None)
            out.append("已击杀，等待巡检补发…")

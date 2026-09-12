"""只击杀「主机自己的」坦克（触发死亡镜头路径），不截图——用于隔离崩溃原因。

崩溃会话的特征：主机坦克死亡 → 阵亡镜头生效 → 约 13s 后 EXCEPTION_ACCESS_VIOLATION。
本脚本复现该路径但不做任何截图，以区分「死亡镜头代码」与「截图 latent action」。
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    server = None
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            server = w
            break

    if server is None:
        out.append("没找到服务端世界")
    else:
        pcs = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController)
        host = None
        for pc in pcs:
            f = getattr(pc, "is_local_player_controller", None)
            if f is not None:
                try:
                    if f():
                        host = pc
                        break
                except Exception:  # noqa: BLE001
                    pass

        host_tank = None
        killer = None
        for t in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn):
            c = t.get_controller()
            if c is None:
                continue
            if c == host:
                host_tank = t
            elif killer is None:
                killer = c

        if host_tank is None:
            out.append("没找到主机坦克")
        else:
            out.append("击杀主机坦克 %s（击杀者 %s）—— 触发死亡镜头路径，不截图"
                       % (host_tank.get_name(), killer.get_name() if killer else "None"))
            unreal.GameplayStatics.apply_damage(host_tank, 9999.0, killer, host_tank, None)
            out.append("已击杀")

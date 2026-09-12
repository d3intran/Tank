"""用「持有 latent 代理」的方式复刻崩溃模式：主机击杀 + 同调用截图 ×3。

对照组是 pie_deathcam_shot.py（直接调用 take_high_res_screenshot，不持有返回对象），
该模式已实测两次在约 50s 后崩溃。
本脚本改用 Content/Python/tank_shot.py 里的持有式调用，用于验证崩溃是否被消除。
"""

import unreal
import tank_shot  # Content/Python 在 sys.path 上（init_unreal.py 同目录）

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
            out.append("击杀主机坦克 %s（击杀者 %s）"
                       % (host_tank.get_name(), killer.get_name() if killer else "None"))
            unreal.GameplayStatics.apply_damage(host_tank, 9999.0, killer, host_tank, None)
            n = tank_shot.shot("hud_deathcam_safe.png")
            out.append("已请求截图（持有式），当前持有代理数 = %d" % n)

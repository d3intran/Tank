"""击杀「本机（主机）」的坦克并立刻截图，验证阵亡镜头修复。

修复前：Pawn 销毁后 APlayerController::CalcCamera 退化成 (PC 自身位置, PC 操控朝向)，
        PC 从未被移动 → 镜头掉到地图原点朝天，整屏只剩天空。
修复后：TankPlayerController 覆盖 GetFocalLocation/GetControlRotation，
        阵亡期间返回「最后的观战位姿」→ 镜头停在残骸处、朝向与坦克一致。
"""

import unreal
import tank_shot  # 持有式截图，见 Content/Python/tank_shot.py

out.clear()


def find_server():
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            return w
    return None


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    server = find_server()
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

        tanks = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn)
        host_tank = None
        killer = None
        for t in tanks:
            c = t.get_controller()
            if c is None:
                continue
            if c == host:
                host_tank = t
            elif killer is None:
                killer = c

        if host_tank is None:
            out.append("没找到主机的坦克（host=%s）" % (host.get_name() if host else "None"))
        else:
            loc = host_tank.get_actor_location()
            out.append("主机 PC=%s 坦克=%s @ (%.0f,%.0f,%.0f)"
                       % (host.get_name(), host_tank.get_name(), loc.x, loc.y, loc.z))
            unreal.GameplayStatics.apply_damage(host_tank, 9999.0, killer, host_tank, None)
            out.append("已击杀，同一次调用内请求截图 hud_deathcam.png")
            tank_shot.shot("hud_deathcam.png")

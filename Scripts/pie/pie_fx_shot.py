"""击杀一辆「非本机」坦克并立刻截图，用于验证 MulticastDeathFX 的调试球 2s 后是否消失。

对照组：等 12s 后再截一张——球没了说明修复生效（原 bPersistentLines=true 会让球一直挂着）。
故意杀非本机坦克，避免死亡镜头接管干扰画面判断。

注意：PC 类改成 TankPlayerController 后 Actor 名变成 TankPlayerController_N，
所以不再按 "PlayerController_N" 字面匹配，改用 is_local_player_controller 找主机。
"""

import unreal
import tank_shot  # 持有式截图，见 Content/Python/tank_shot.py

out.clear()


def host_and_others(server):
    pcs = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController)
    host = None
    others = []
    for pc in pcs:
        f = getattr(pc, "is_local_player_controller", None)
        is_host = False
        if f is not None:
            try:
                is_host = bool(f())
            except Exception:  # noqa: BLE001
                is_host = False
        (others.append(pc) if not is_host else None)
        if is_host:
            host = pc
    return host, others


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
        host, others = host_and_others(server)
        out.append("主机 PC = %s；其他 PC = %s"
                   % (host.get_name() if host else "None",
                      ", ".join(p.get_name() for p in others)))

        # 按 Controller 找坦克
        tanks = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn)
        tank_of = {}
        for t in tanks:
            c = t.get_controller()
            if c is not None:
                tank_of[c] = t

        if len(others) < 2:
            out.append("非主机玩家不足 2 个，无法构造『A 杀 B』")
        else:
            killer = others[0]
            victim = tank_of.get(others[1])
            if victim is None:
                out.append("找不到受害者坦克")
            else:
                loc = victim.get_actor_location()
                out.append("击杀 %s @ (%.0f,%.0f,%.0f)（击杀者 %s）"
                           % (victim.get_name(), loc.x, loc.y, loc.z, killer.get_name()))
                unreal.GameplayStatics.apply_damage(victim, 9999.0, killer, victim, None)
                tank_shot.shot("fx_now.png")
                out.append("已请求截图 fx_now.png（此刻爆炸球应在场）")

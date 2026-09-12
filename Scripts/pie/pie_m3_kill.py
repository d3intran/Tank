"""M3 击杀测试：让「玩家A 打死 玩家B」，验证击杀计分真的落到击杀者头上。

与 pie_kill.py 的区别：pie_kill.py 用受害者自己的 Controller 当 Instigator，那是**自杀**，
按规则不计击杀分。这里显式把 event_instigator 设成**另一个玩家**的 Controller。

注意：PC 类改为 TankPlayerController 后 Actor 名是 TankPlayerController_N，
因此不按 "PlayerController_N" 字面匹配，改用 is_local_player_controller 找主机 + 序号选其他玩家。
"""

import unreal

out.clear()


def find_server():
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            return w
    return None


def is_local(pc):
    f = getattr(pc, "is_local_player_controller", None)
    if f is None:
        return False
    try:
        return bool(f())
    except Exception:  # noqa: BLE001
        return False


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行——先跑 pie_start.py")
else:
    server = find_server()
    if server is None:
        out.append("没找到服务端世界")
    else:
        pcs = [p for p in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController)
               if not is_local(p)]
        tanks = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn)

        # Controller -> Tank
        tank_of = {}
        for t in tanks:
            c = t.get_controller()
            if c is not None:
                tank_of[c] = t

        # 优先挑两个「非主机」玩家构造 A 杀 B，避免动到本机镜头
        alive = [p for p in pcs if p in tank_of]
        if len(alive) >= 2:
            killer_ctrl, victim_ctrl = alive[0], alive[1]
        elif len(alive) == 1:
            killer_ctrl, victim_ctrl = alive[0], None
        else:
            killer_ctrl, victim_ctrl = None, None

        if killer_ctrl is None or victim_ctrl is None:
            out.append("可用玩家不足（非主机且有坦克的 PC 数=%d）" % len(alive))
        else:
            victim = tank_of[victim_ctrl]
            loc = victim.get_actor_location()
            out.append("击杀者 = %s" % killer_ctrl.get_name())
            out.append("受害者 = %s @ (%.0f,%.0f,%.0f)" % (victim.get_name(), loc.x, loc.y, loc.z))
            unreal.GameplayStatics.apply_damage(victim, 9999.0, killer_ctrl, victim, None)
            out.append("已施加 9999 伤害（Instigator=击杀者），等待结算…")

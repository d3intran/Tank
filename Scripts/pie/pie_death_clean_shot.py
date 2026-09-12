"""清掉之前强制的胜利状态，再击杀本机坦克并截图，拿到干净的「阵亡/重生」提示画面。"""

import unreal
import tank_shot  # 持有式截图，见 Content/Python/tank_shot.py

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
        # 1) 还原被强制设置的胜利状态
        for gs in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankGameState):
            gs.set_editor_property("match_over", False)
            gs.set_editor_property("winner_name", "")
        out.append("已还原 MatchOver=False")

        # 2) 打死本机的坦克
        victim = None
        killer = None
        for t in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn):
            c = t.get_controller()
            if c is None:
                continue
            if c.get_name().endswith("Controller_0"):
                victim = t
            elif killer is None:
                killer = c

        if victim is None:
            out.append("没找到主机的坦克")
        else:
            unreal.GameplayStatics.apply_damage(victim, 9999.0, killer, victim, None)
            out.append("击杀本机坦克 %s" % victim.get_name())
            # 3) 同一次调用里请求截图
            tank_shot.shot("hud_death_clean.png")
            out.append("截图已请求 (hud_death_clean.png)")

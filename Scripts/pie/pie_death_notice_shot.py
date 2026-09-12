"""击杀本机坦克并在同一次桥调用里发起截图，尽量抓到「坦克被击毁 / 正在重生…」提示。

阵亡提示的可见窗口 = 销毁(死后 0.2s) → 占有巡检补发(≤2s)，只有约 2 秒。
分开两次桥调用（各约 1.5~2s 往返）大概率错过，所以在同一个脚本里先击杀、再立刻请求截图。
"""

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
        # 找主机的坦克（PlayerController_0），把它打死
        victim = None
        for t in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn):
            c = t.get_controller()
            if c is not None and c.get_name().endswith("Controller_0"):
                victim = t
                break

        if victim is None:
            out.append("没找到主机的坦克")
        else:
            # 用另一个玩家当击杀者，避免自杀分支
            killer = None
            for t in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn):
                c = t.get_controller()
                if c is not None and not c.get_name().endswith("Controller_0"):
                    killer = c
                    break

            out.append("击杀本机坦克 %s（击杀者 %s）" % (victim.get_name(),
                                                     killer.get_name() if killer else "None"))
            unreal.GameplayStatics.apply_damage(victim, 9999.0, killer, victim, None)

            # 同一次调用里立刻请求截图：捕获发生在后续帧，正好落在阵亡窗口内
            try:
                tank_shot.shot("hud_death.png")
                out.append("截图已请求 (hud_death.png)")
            except Exception as exc:  # noqa: BLE001
                out.append("截图请求失败: %s" % exc)

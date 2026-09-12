"""在 PIE 服务端世界里把「客户端玩家的坦克」打死，用来验证 M2 重生占有链路。

为什么需要它：
  pie_inspect.py 验证的是**开局**占有（3 车 3 PC 无孤儿）。
  而原 bug 的触发条件是「为客户端重生而 Spawn 坦克」——必须先让一辆车死掉。
  本脚本只负责开火/致死，之后需在编辑器外 sleep 几秒，再跑一次 pie_inspect.py 看结果。

用法（明天一条命令）：
  cd E:/UE/Tank
  deno run -A Scripts/editor.deno.ts execfile Scripts/pie/pie_kill.py --timeout=60
  sleep 8
  deno run -A Scripts/editor.deno.ts execfile Scripts/pie/pie_inspect.py --timeout=90

判定标准：
  - 服务端世界仍有 3 辆车、每辆都有 Controller（孤儿 0）→ 修复生效
  - 若主机（PlayerController_0）的坦克变成孤儿、或出现两车堆叠 → 修复未生效
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未在运行——先跑 pie_start.py")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)

    # 服务端世界 = 唯一拥有多个 PlayerController 的那个
    server = None
    for w in worlds:
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            server = w
            break

    if server is None:
        out.append("没找到服务端世界（PC 数 > 1 的世界）")
    else:
        tanks = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn)
        out.append("服务端世界 %s：Tank=%d" % (server.get_name(), len(tanks)))

        # 挑一辆「客户端玩家的」坦克：其 Controller 不是 PlayerController_0（主机）
        victim = None
        for t in tanks:
            c = t.get_controller()
            if c is not None and not c.get_name().endswith("Controller_0"):
                victim = t
                break
        if victim is None and tanks:
            victim = tanks[-1]

        if victim is None:
            out.append("没有可击杀的坦克")
        else:
            ctrl = victim.get_controller()
            out.append("目标：%s (controller=%s) 位置=%s"
                       % (victim.get_name(), ctrl.get_name() if ctrl else "NONE",
                          victim.get_actor_location()))
            # 血量 1000，直接给 9999 保证致死
            unreal.GameplayStatics.apply_damage(victim, 9999.0, ctrl, victim, None)
            out.append("已施加 9999 点伤害，等待 GameMode 巡检（2s 周期）补发新坦克…")

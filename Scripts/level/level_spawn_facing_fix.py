"""把 8 个 PlayerStart 的朝向修正为「面向场地中心 (0,4600)」。

历史 bug：level_ffa_setup.py / m0_place_spawns.py 用 `unreal.Rotator(0.0, yaw, 0.0)` 位置传参，
而本机 Python 绑定的位置顺序是 (roll, pitch, yaw) → 想要的 yaw 被塞进了 pitch、真正的 yaw=0，
出生点全部朝向错误（实测 probe_spawns.py：8/8 偏）。本脚本一次性修正并保存。
"""

import math

import unreal

out.clear()
CENTER = (0.0, 4600.0)
starts = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
          if isinstance(a, unreal.PlayerStart)]
out.append("PlayerStart %d 个" % len(starts))
for a in starts:
    l = a.get_actor_location()
    want = math.degrees(math.atan2(CENTER[1] - l.y, CENTER[0] - l.x))
    a.set_actor_rotation(unreal.Rotator(pitch=0.0, yaw=want, roll=0.0), False)
    r = a.get_actor_rotation()
    out.append("  %-16s yaw -> %7.1f（读回 %7.1f）%s"
               % (a.get_actor_label(), want, r.yaw,
                  "OK" if abs(((r.yaw - want + 180) % 360) - 180) < 2 else "!!"))
out.append("")
out.append("保存关卡: %s" % unreal.EditorLevelLibrary.save_current_level())

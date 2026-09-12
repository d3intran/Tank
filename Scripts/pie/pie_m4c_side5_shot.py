"""坡中 bug 侧视取景 · 出图。"""

import unreal

import tank_shot

out.clear()

n = tank_shot.shot("ramp_pitch_bug_side5.png")
out.append("已请求 ramp_pitch_bug_side5.png（已持有代理 %d 个）" % n)

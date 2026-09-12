"""确认 unreal.Rotator 构造参数顺序（位置参数 vs 关键字）。"""

import unreal

out.clear()

r1 = unreal.Rotator(1.0, 2.0, 3.0)
out.append("位置 (1,2,3) → pitch=%.1f yaw=%.1f roll=%.1f | repr=%s"
           % (r1.pitch, r1.yaw, r1.roll, str(r1)))

r2 = unreal.Rotator(pitch=4.0, yaw=5.0, roll=6.0)
out.append("关键字 pitch=4 yaw=5 roll=6 → pitch=%.1f yaw=%.1f roll=%.1f | repr=%s"
           % (r2.pitch, r2.yaw, r2.roll, str(r2)))

r3 = unreal.Rotator(roll=7.0, pitch=8.0, yaw=9.0)
out.append("关键字 roll=7 pitch=8 yaw=9 → pitch=%.1f yaw=%.1f roll=%.1f"
           % (r3.pitch, r3.yaw, r3.roll))

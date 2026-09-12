"""探针：钉死 unreal.Rotator 的构造参数顺序与 UE 的 pitch 语义。

背景：level_center_ramp.py 用 Rotator(pitch, yaw) 给坡道定向，实测读回的 pitch/yaw
与设置值对不上（坡度显示 90°）。这里用最小用例 + 「局部 +X 轴指到哪」直接看结果。
"""

import math

import unreal

out.clear()

cases = [
    ("positional (25, 90, 0)", unreal.Rotator(25.0, 90.0, 0.0)),
    ("keyword (pitch=25, yaw=90, roll=0)", unreal.Rotator(pitch=25.0, yaw=90.0, roll=0.0)),
    ("keyword (roll=25, pitch=90, yaw=0)", unreal.Rotator(roll=25.0, pitch=90.0, yaw=0.0)),
]

for label, rot in cases:
    out.append("--- %s" % label)
    out.append("    直接读: pitch=%.1f yaw=%.1f roll=%.1f" % (rot.pitch, rot.yaw, rot.roll))
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 300.0), rot)
    if a is None:
        out.append("    !! spawn 失败")
        continue
    r = a.get_actor_rotation()
    fwd = a.get_actor_forward_vector()
    upv = a.get_actor_up_vector()
    out.append("    spawn 后读回: pitch=%.1f yaw=%.1f roll=%.1f" % (r.pitch, r.yaw, r.roll))
    out.append("    ActorForward = (%.3f, %.3f, %.3f)   ActorUp = (%.3f, %.3f, %.3f)"
               % (fwd.x, fwd.y, fwd.z, upv.x, upv.y, upv.z))
    a.destroy_actor()

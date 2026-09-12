"""诊断：Actor yaw 能否被脚本设置 / 会不会被之后的 tick 重置。

流程：读原 yaw → 三种方式各试一次并立刻回读 → 写入结果文件。
再运行一次本脚本可看出「设过的 yaw 是否在若干帧后被重置」。
"""

import unreal

YAW_TEST = 77.0
out.clear()

report = []


def local_pairs(world):
    pairs = []
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                pairs.append((pc, pc.get_controlled_pawn()))
        except Exception:  # noqa: BLE001
            pass
    return pairs


def read_yaw(t):
    return t.get_actor_rotation().yaw


worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
server = max(worlds, key=lambda w: len(
    unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))

for w in sorted(worlds, key=lambda w: w.get_name()):
    tag = "SERVER" if w is server else "CLIENT"
    for _pc, t in local_pairs(w):
        name = t.get_name()
        loc = t.get_actor_location()
        report.append("%s %s 初始 yaw=%.1f" % (tag, name, read_yaw(t)))

        # 方式1：组合 API（teleport=True）
        t.set_actor_location_and_rotation(
            loc, unreal.Rotator(0.0, YAW_TEST, 0.0), False, True)
        report.append("  方式1 set_actor_location_and_rotation(yaw=%.0f) → 回读 %.1f"
                      % (YAW_TEST, read_yaw(t)))

        # 方式2：set_actor_rotation 带 sweep
        t.set_actor_rotation(unreal.Rotator(0.0, YAW_TEST + 10.0, 0.0), True)
        report.append("  方式2 set_actor_rotation(yaw=%.0f, sweep) → 回读 %.1f"
                      % (YAW_TEST + 10.0, read_yaw(t)))

        # 方式3：根组件世界旋转
        try:
            root = t.get_editor_property("root_component")
            root.set_world_rotation(unreal.Rotator(0.0, YAW_TEST + 20.0, 0.0))
            report.append("  方式3 root.set_world_rotation(yaw=%.0f) → 回读 %.1f"
                          % (YAW_TEST + 20.0, read_yaw(t)))
        except Exception as exc:  # noqa: BLE001
            report.append("  方式3 失败: %s" % exc)

        # 收尾：设回 77 供下一次运行对比
        t.set_actor_location_and_rotation(
            loc, unreal.Rotator(0.0, YAW_TEST, 0.0), False, True)

with open(r"E:\UE\Tank\Scripts\_yaw_probe.txt", "w", encoding="utf-8") as fh:
    fh.write("\n".join(report))
out.append("done")

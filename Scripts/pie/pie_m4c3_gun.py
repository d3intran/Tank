"""M4c-3 · 俯角实测/设置：把 Client1 本机车的目标俯仰设到指定角度（读 Scripts/_gun.txt），
报告是否写入成功、当前俯仰、炮口世界位置与炮管前向；配合截图判断炮管会不会切进车体。"""

import os

import unreal

out.clear()

GUN = os.path.join(unreal.Paths.project_dir(), "Scripts", "_gun.txt")
with open(GUN, "r", encoding="utf-8") as f:
    want = float(f.read().strip())


def local_tank(world):
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                return pc.get_controlled_pawn()
        except Exception:  # noqa: BLE001
            pass
    return None


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
    tank = None
    for w in sorted([w for w in worlds if w is not server], key=lambda w: w.get_name()):
        tank = local_tank(w)
        if tank is not None:
            break
    if tank is None:
        out.append("没找到客户端本机车")
    else:
        ok = "-"
        for name in ("desired_gun_pitch", "DesiredGunPitch"):
            try:
                tank.set_editor_property(name, want)
                ok = name
                break
            except Exception as exc:  # noqa: BLE001
                ok = "%s: %s" % (name, exc)
        out.append("写入 %s → %s" % (want, ok))
        try:
            out.append("读回 desired_gun_pitch=%s current_pitch=%s min_pitch=%s" % (
                tank.get_editor_property("desired_gun_pitch"),
                tank.get_editor_property("current_pitch"),
                tank.get_editor_property("min_pitch")))
        except Exception as exc:  # noqa: BLE001
            out.append("读回失败: %s" % exc)
        gp = getattr(tank, "gun_pivot", None)
        gm = getattr(tank, "gun_mesh", None)
        hull = getattr(tank, "hull_mesh", None)
        if gp is not None:
            p = gp.get_world_location()
            out.append("火炮轴心 (%.1f,%.1f,%.1f)" % (p.x, p.y, p.z))
        if gm is not None:
            p = gm.get_world_location()
            f = gm.get_forward_vector()
            out.append("炮管原点 (%.1f,%.1f,%.1f) 前向 (%.2f,%.2f,%.2f) → 下俯 %.1f°"
                       % (p.x, p.y, p.z, f.x, f.y, f.z,
                          -unreal.MathLibrary.radians_to_degrees(unreal.MathLibrary.asin(f.z))))
        if hull is not None:
            try:
                ob = hull.get_bounds()
                out.append("车体网格包围盒 Z∈[%.1f,%.1f] X∈[%.1f,%.1f]" % (
                    ob.origin.z - ob.box_extent.z, ob.origin.z + ob.box_extent.z,
                    ob.origin.x - ob.box_extent.x, ob.origin.x + ob.box_extent.x))
            except Exception as exc:  # noqa: BLE001
                out.append("包围盒失败: %s" % exc)

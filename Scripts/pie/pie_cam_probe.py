"""诊断：本机视口真实朝向 —— 车体 yaw vs 相机 forward，以及正前方打到的物件。"""

import unreal

out.clear()


def local_tanks(world):
    result = []
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                result.append((pc, pc.get_controlled_pawn()))
        except Exception:  # noqa: BLE001
            pass
    return result


def trace(world, start, end):
    try:
        hit = unreal.SystemLibrary.line_trace_single(
            world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
            False, [], unreal.DrawDebugTrace.NONE, True)
        t = hit.to_tuple()
        return "%s @ (%.0f,%.0f,%.0f)" % (t[9] or t[11] or t[10] or t[12] or "?",
                                         t[5].x, t[5].y, t[5].z)
    except Exception as exc:  # noqa: BLE001
        return "trace 失败: %s" % exc


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
    for w in sorted(worlds, key=lambda w: w.get_name()):
        tag = "SERVER" if w is server else "CLIENT"
        for pc, t in local_tanks(w):
            loc = t.get_actor_location()
            rot = t.get_actor_rotation()
            try:
                sa = t.get_editor_property("SpringArm")
                cam = t.get_editor_property("Camera")
            except Exception as exc:  # noqa: BLE001
                out.append("%s %s 取组件失败: %s" % (tag, t.get_name(), exc))
                continue
            sa_rot = sa.get_world_rotation()
            cam_loc = cam.get_world_location()
            f = cam.get_forward_vector()
            ahead = trace(w, cam_loc, cam_loc + f * 4000.0)
            to_bug = cam_loc + (unreal.Vector(850.0, 3000.0, 368.0) - cam_loc)
            out.append(
                "%s %s actor=(%.0f,%.0f,%.0f) yaw=%.1f pitch=%.1f | armYaw=%.1f armPitch=%.1f "
                "camLoc=(%.0f,%.0f,%.0f) camFwd=(%.2f,%.2f,%.2f) | 前方4m: %s"
                % (tag, t.get_name(), loc.x, loc.y, loc.z, rot.yaw, rot.pitch,
                   sa_rot.yaw, sa_rot.pitch, cam_loc.x, cam_loc.y, cam_loc.z,
                   f.x, f.y, f.z, ahead))
            out.append("    视线到 bug 车位 (850,3000,368) 前方打点: %s"
                       % trace(w, cam_loc, to_bug))

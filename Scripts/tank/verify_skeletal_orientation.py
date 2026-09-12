"""判定"骨骼网格的 UE 朝向是否与现有静态网格一致"。

背景：OBJ 是 UE 空间数据，经 Blender 往返后在 UE 里 Y 轴可能被镜像（右手系↔UE 左手系的换算）。
本探针以**现有静态网格资产**（ztz88a_hull_body / ztz88a_tracks_full，用户已确认的观感）为基准真值，
逐个比对包围盒，避免在坐标换算链上做符号推理。
同时探测骨位读取 API（Skeleton.get_reference_pose / Socket 构造）。
"""

import unreal

out.clear()

MESHES = [
    "/Game/tank/ztz-88a/ztz88a_hull_body.ztz88a_hull_body",
    "/Game/tank/ztz-88a/ztz88a_tracks_full.ztz88a_tracks_full",
    "/Game/tank/ztz-88a/ztz88a_skeletal.ztz88a_skeletal",
]


def spawn_bounds(path):
    asset = unreal.load_asset(path)
    if asset is None:
        return None, "加载失败"
    actor = None
    if isinstance(asset, unreal.SkeletalMesh):
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        actor.get_editor_property("skeletal_mesh_component").set_skeletal_mesh(asset)
    else:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        comp = actor.get_editor_property("static_mesh_component")
        comp.set_static_mesh(asset)
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    origin, extent = actor.get_actor_bounds(False)
    bounds = ((origin.x - extent.x, origin.y - extent.y, origin.z - extent.z),
              (origin.x + extent.x, origin.y + extent.y, origin.z + extent.z))
    unreal.EditorLevelLibrary.destroy_actor(actor)
    return bounds, None


for path in MESHES:
    bounds, err = spawn_bounds(path)
    if err:
        out.append("%-28s %s" % (path.split("/")[-1], err))
        continue
    out.append("%-28s min=(%.1f, %.1f, %.1f) max=(%.1f, %.1f, %.1f)"
               % (path.split("/")[-1], bounds[0][0], bounds[0][1], bounds[0][2],
                  bounds[1][0], bounds[1][1], bounds[1][2]))

# ---- 骨位读取 API 探测 ----
mesh = unreal.load_asset("/Game/tank/ztz-88a/ztz88a_skeletal.ztz88a_skeletal")
skel = mesh.get_editor_property("skeleton")
out.append("Skeleton.get_reference_pose 存在: %s" % hasattr(skel, "get_reference_pose"))
try:
    pose = skel.get_reference_pose()
    out.append("  get_reference_pose() → %s" % type(pose))
    out.append("  dir: %s" % [m for m in dir(pose) if not m.startswith("_")][:20])
    try:
        out.append("  to_dict: %s" % str(pose.to_dict())[:400])
    except Exception as exc:  # noqa: BLE001
        out.append("  to_dict 失败: %s" % exc)
except Exception as exc:  # noqa: BLE001
    out.append("  get_reference_pose() 失败: %s" % exc)

out.append("unreal.SkeletalMeshSocket 可构造类: %s" % hasattr(unreal, "SkeletalMeshSocket"))
try:
    sock = unreal.new_object(unreal.SkeletalMeshSocket)
    sock.set_editor_property("socket_name", "probe_tmp")
    sock.set_editor_property("bone_name", "wheel_r0")
    sock.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
    mesh.add_socket(sock)
    out.append("add_socket(对象) 成功")
    mesh.remove_socket(sock)
    out.append("remove_socket 成功")
except Exception as exc:  # noqa: BLE001
    out.append("Socket 对象路线失败: %s" % exc)

"""把坦克部件（车体 + 履带）合成一个骨骼网格，供 UE5 Chaos 载具使用。

为什么必须做这一步：`UChaosWheeledVehicleMovementComponent` 只能挂在 `USkeletalMeshComponent` 上，
且轮子的物理位置来自骨骼（`FChaosWheelSetup::BoneName` → `LocateBoneOffset` 取骨骼 ref pose 位置）。
现有 13 个静态网格组件挂不上去。

设计（与 TankPawn.cpp 的现有布局**严格一致**，便于逐项对照/回归）：
  - 骨骼：root（网格原点 = 履带底面）+ 12 根负重轮骨（位置 = RoadSetups 表 + RoadWheelY/Z 常量）
  - 几何：hull_body + tracks 并成一个对象，全部刚性绑到 root 骨（权重 1，无需蒙皮权重）
  - 负重轮 / 炮塔 / 火炮**不进**骨骼网格：沿用现有静态网格组件（材质、履带 UV 滚动全部保留）
    负重轮骨只是给物理模拟定位用的空骨（无几何）。

坐标约定（实测验证，见下）：
  - OBJ 存的是 UE 空间（X 前 / Y 右 / Z 上，单位 cm），但 OBJ 惯例是 Y-up，
    Blender 导入时按 Y-up 解读 → 数据落成 bl = (X, -Z, Y)，坦克"上"变成 Blender -Y。
  - 因此导入后要把整个装配体绕 X 轴 -90° 摆正（bl' = (x, z, -y)），坦克"上"回到 Blender +Z；
    这一步之后 bl' 与 UE 坐标**数值相同**（X 前 / Y 右 / Z 上），骨头就按 UE 坐标直接写。
  - FBX 导出用默认（Blender Z-up → FBX Y-up），UE 导入端做右手→左手换算，实测往返后
    UE 空间坐标为 (X, -Y, Z)：长度/高度/宽度都在对的轴上，Y 取负正是右手系→UE 左手系的必然镜像。
  - 端到端实测：src → UE = 恒等（包围盒逐轴对齐，见 import_tank_skeletal.py 的断言）。

单位（**踩过的坑，别再动**）：
OBJ 的 1 单位 = 1cm。Blender 的 FBX 导出默认会把「场景米」换算成「FBX 厘米」，
代价是给**骨架根节点写一个 ×100 的缩放** —— UE 侧表现为 `TankArmature` 骨 scale=100，
于是物理资产的碰撞盒被放大 50 倍（0.5 组件缩放 × 100 骨缩放）、轮子的物理位置全错。
所以本脚本**全程不做单位换算**：
  - OBJ 导入 global_scale=1.0（数值原样进 Blender：坦克长 639 单位）
  - Blender 场景单位设为厘米（unit_settings.scale_length = 0.01）：导出器的「米→厘米」
    换算变成恒等，骨架节点缩放 = 1
  - FBX 导出保留默认的 apply_unit_scale=True（此时它就是空操作）
UE 导入端 1 单位 = 1cm，端到端 1:1。

用法：
    "/c/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background \
        --python Scripts/build_tank_skeleton.py
"""

import math
import os
import sys

import bpy
import mathutils

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(SCRIPT_DIR)
SRC_DIR = os.path.join(PROJ, "Content", "tank", "ztz-88a")
OUT_DIR = os.path.join(PROJ, "Import", "tank_skeletal")
OUT_FBX = os.path.join(OUT_DIR, "ztz88a_skeletal.fbx")

MESH_SOURCES = [
    ("ztz88a_hull_body.obj", "MAT_HULL"),
    ("ztz88a_tracks.obj", "MAT_TRACKS"),
]

# ---- 与 TankPawn.cpp 严格同步的布局常量（网格空间，cm）----
WHEEL_X = [-206.00, -131.00, -56.00, 19.00, 107.50, 199.95]
WHEEL_Y = 144.0
WHEEL_Z = 41.75
WHEEL_BONE_LEN = 40.0
ROOT_BONE_LEN = 60.0

# 摆正矩阵：Blender RX(-90°)。OBJ 导入落在 (X,-Z,Y)，乘上它之后回到 (X,Y,Z)（= UE 坐标）
LEVEL_ROTATION = mathutils.Matrix.Rotation(math.radians(-90.0), 4, "X")

ROOT_BONE_NAME = "root"


def wheel_defs():
    """返回 [(bone_name, bl_pos)]，顺序与 TankPawn 的 RoadSetups 一致：0-5 右，6-11 左。

    左右语义基准是 UE 空间（右 = UE +Y）。实测导出链对**骨骼位置**的净映射是 (x, y, z) → (x, −y, z)
    （导出器写骨骼时按自己的约定又转/镜像了一次；网格顶点不受影响，走 axis_forward/axis_up 声明）。
    所以在 Blender 里把 r 侧写在 **−Y**、l 侧写在 **+Y**，UE 侧 r 才落在 +Y：
      wheel_r0 组件空间 = (−206, 144, 41.75) ✓（导入脚本会断言这一条）。
    """
    out = []
    for i, x in enumerate(WHEEL_X):
        out.append(("wheel_r%d" % i, (x, -WHEEL_Y, WHEEL_Z)))
    for i, x in enumerate(WHEEL_X):
        out.append(("wheel_l%d" % i, (x, WHEEL_Y, WHEEL_Z)))
    return out


def log(msg):
    print("[tank-skel] %s" % msg)


def import_meshes():
    objs = []
    for filename, mat_name in MESH_SOURCES:
        path = os.path.join(SRC_DIR, filename)
        before = set(bpy.data.objects)
        # global_scale=1.0：数值原样进 Blender（OBJ 的 1 单位就是 1cm，见模块 docstring 的单位段）
        bpy.ops.wm.obj_import(filepath=path, global_scale=1.0)
        new_objs = [o for o in bpy.data.objects if o not in before]
        log("导入 %s → %s" % (filename, [o.name for o in new_objs]))
        for o in new_objs:
            # 统一材质名，导入 UE 后据此认槽（SOP-2：重导会重置材质槽）
            if o.type == "MESH":
                for slot in o.material_slots:
                    if slot.material:
                        slot.material.name = mat_name
                objs.append(o)
    return objs


def join_meshes(objs):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    body = bpy.context.view_layer.objects.active
    body.name = "TankBody"
    body.data.name = "TankBodyMesh"
    log("合并后对象 %s：顶点 %d 面 %d 材质 %s"
        % (body.name, len(body.data.vertices), len(body.data.polygons),
           [m.name for m in body.data.materials]))
    return body


def level_assembly(body):
    """把导入后的装配体绕 X 轴 -90° 摆正：Blender 空间里坦克"上"从 -Y 回到 +Z。

    做完这一步，Blender 坐标的**数值**就等于 UE 坐标（X 前 / Y 右 / Z 上），骨头可以直接按 UE 坐标写。
    """
    body.matrix_world = LEVEL_ROTATION @ body.matrix_world
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)


def build_armature():
    arm_data = bpy.data.armatures.new("TankSkeleton")
    arm_obj = bpy.data.objects.new("TankArmature", arm_data)
    bpy.context.collection.objects.link(arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    arm_obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")

    # 摆正之后 Blender 坐标 = UE 坐标，骨头直接按 UE 坐标写（见 LEVEL_ROTATION）
    #
    # ★ root 骨必须沿 **+X（车头方向）** ★
    # 物理刚体绑在 root 骨上：一旦 root 骨的 ref pose 带旋转，整个刚体空间就跟着转
    # （实测整车 roll=90°：视觉躺倒、悬挂射线朝侧面扫、给油不走）。
    # 这里 root 骨朝 Blender 的 +Y —— 抵消导出器的骨骼 −90° 偏航后，UE 侧正好是 +X（车头）且无旋转。
    root = arm_data.edit_bones.new(ROOT_BONE_NAME)
    root.head = (0.0, 0.0, 0.0)
    root.tail = (0.0, ROOT_BONE_LEN, 0.0)   # +Y_blender（预转 +90° 后 = UE 的 +X）
    root.roll = 0.0

    for name, ue_pos in wheel_defs():
        bone = arm_data.edit_bones.new(name)
        bone.head = ue_pos
        bone.tail = (ue_pos[0], ue_pos[1], ue_pos[2] + WHEEL_BONE_LEN)
        bone.roll = 0.0
        bone.parent = root
        bone.use_connect = False

    bpy.ops.object.mode_set(mode="OBJECT")

    log("骨骼 %d 根（root + 12 轮；括号内为导入 UE 后的坐标 = X, -Y, Z）：" % len(arm_data.bones))
    for bone in arm_data.bones:
        h = bone.head_local
        log("  %-10s bl=(%.2f, %.2f, %.2f) → ue=(%.2f, %.2f, %.2f)"
            % (bone.name, h[0], h[1], h[2], h[0], -h[1], h[2]))
    return arm_obj


def bind_rigid(body, arm_obj):
    """刚性绑定：整车顶点全部以权重 1 绑到 root 骨。"""
    group = body.vertex_groups.new(name=ROOT_BONE_NAME)
    group.add([v.index for v in body.data.vertices], 1.0, "REPLACE")

    body.parent = arm_obj
    body.matrix_parent_inverse = arm_obj.matrix_world.inverted()
    mod = body.modifiers.new(name="Armature", type="ARMATURE")
    mod.object = arm_obj
    mod.use_vertex_groups = True
    log("刚性绑定完成：%d 顶点 → 骨 %s" % (len(body.data.vertices), ROOT_BONE_NAME))


def export_fbx():
    os.makedirs(OUT_DIR, exist_ok=True)
    props = bpy.ops.export_scene.fbx.get_rna_type().properties.keys()
    log("FBX 导出可用参数示例: %s" % [p for p in props if "smooth" in p or "scale" in p
                                       or "leaf" in p or "anim" in p or "axis" in p])
    bpy.ops.export_scene.fbx(
        filepath=OUT_FBX,
        use_selection=False,
        object_types={"ARMATURE", "MESH"},
        add_leaf_bones=False,
        bake_anim=False,
        mesh_smooth_type="OFF",
        path_mode="AUTO",
        # ★★ 关掉轴转换（不要用默认的 Y-up）★★
        # 默认导出会把「Blender Z-up → FBX Y-up」那次 −90°X 旋转**烘到骨架根节点上**；
        # UE 导入后它变成骨架的第一根骨 "TankArmature"，带着这个旋转 → 绑在 root 骨上的
        # 物理刚体空间整体转了 90°：整车 roll=90°（视觉躺倒）、悬挂射线朝侧面扫（永远探不到地）、
        # 驱动力方向错乱。实测取证：车 actor roll=90.3°。
        # 我们的数据本来就是 X 前 / Y 右 / Z 上（脚本内已摆正），所以直接声明同一套轴，
        # 导出零旋转、UE 侧也零旋转（端到端仍是 1:1）。
        axis_forward="X",
        axis_up="Z",
    )
    log("导出 %s (%d 字节)" % (OUT_FBX, os.path.getsize(OUT_FBX)))


def measure(obj):
    """返回对象在世界空间的包围盒（Blender 空间，米）。"""
    xs = [obj.matrix_world @ v.co for v in obj.data.vertices]
    mn = [min(v[i] for v in xs) for i in range(3)]
    mx = [max(v[i] for v in xs) for i in range(3)]
    return mn, mx


# 摆正后期望的包围盒（= OBJ 源数据的 UE 坐标，1 单位 = 1cm，见模块 docstring）
EXPECTED_BBOX_CM = ((-350.7, -177.1, 0.0), (338.7, 181.2, 153.5))


def assert_bbox(body):
    mn, mx = measure(body)
    cm_mn = list(mn)
    cm_mx = list(mx)
    log("摆正后包围盒 cm: min=(%.1f, %.1f, %.1f) max=(%.1f, %.1f, %.1f)"
        % (cm_mn[0], cm_mn[1], cm_mn[2], cm_mx[0], cm_mx[1], cm_mx[2]))
    for i in range(3):
        if abs(cm_mn[i] - EXPECTED_BBOX_CM[0][i]) > 1.0 or abs(cm_mx[i] - EXPECTED_BBOX_CM[1][i]) > 1.0:
            raise SystemExit("[tank-skel] 包围盒断言失败：轴 %d 实测 [%.1f, %.1f] 期望 [%.1f, %.1f]"
                             % (i, cm_mn[i], cm_mx[i],
                                EXPECTED_BBOX_CM[0][i], EXPECTED_BBOX_CM[1][i]))
    log("包围盒断言通过（与 OBJ 源数据逐轴一致；Y 轴不对称方向 = 右手系镜像后的结果）")


def main():
    log("Blender %s" % bpy.app.version_string)
    bpy.ops.wm.read_factory_settings(use_empty=True)

    # 场景单位 = 厘米：让 FBX 导出器的「米 → 厘米」换算成为恒等操作（见模块 docstring 的单位段）。
    # 不设这个，导出器会写一个 ×100 的骨架根节点缩放，UE 侧物理碰撞盒会被放大 50 倍。
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 0.01
    log("场景单位 scale_length=%.3f（1 Blender 单位 = 1cm）" % scene.unit_settings.scale_length)

    objs = import_meshes()
    body = join_meshes(objs)
    level_assembly(body)
    assert_bbox(body)
    arm_obj = build_armature()
    bind_rigid(body, arm_obj)
    export_fbx()
    log("OK")


main()

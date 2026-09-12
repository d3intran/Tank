"""M4：把场地 FFA 化 —— 8 个环形出生点 + 中央/四角掩体。

依据（level_recon / level_bounds 实测）：
  地面 SM_Template_Map_Floor：X -3189~3189，Y -7864~7624，顶面 Z ≈ -40
  城门 CityGate_Wall 占 Y ∈ [-409, 1339]（南侧不可通行），所以可用战场 ≈ X ±3100、Y 1500~7500
  原有出生点只有 3 个且不均布（两个 r=2110、一个 r=1100）

设计：
  出生环：圆心 (0, 4600)，半径 2800，8 个点按 22.5° 起偏均布，全部朝圆心
  掩体：中央一道断裂矮墙（3 块）+ 四角 4 个方块，全部在出生环内侧，避免压到出生点

坐标系说明：本关地面是 XY 平面、Z 向上；Cube 基础网格为 100cm，缩放 = 目标尺寸/100。
"""

import unreal

out.clear()

CENTER_X, CENTER_Y = 0.0, 4600.0
RING_RADIUS = 2800.0
SPAWN_Z = 123.0            # 与原有出生点同高
ANGLE_OFFSET = 22.5        # 让出生点错开掩体所在的轴向/对角
COVER_Z_BASE = -40.0       # 地面顶面

ws = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
out.append("世界: %s" % ws.get_name())

cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube")
if cube is None:
    out.append("!! 找不到 /Engine/BasicShapes/Cube，无法建掩体")
mat = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
out.append("Cube=%s  Material=%s" % (cube.get_name() if cube else None,
                                     mat.get_name() if mat else None))


def make_cover(label, cx, cy, sx, sy, sz, yaw=0.0):
    """在 (cx, cy) 生成一块 sx×sy×sz(cm) 的方形掩体，底面贴地。"""
    loc = unreal.Vector(cx, cy, COVER_Z_BASE + sz * 0.5)
    rot = unreal.Rotator(0.0, yaw, 0.0)
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
    if a is None:
        out.append("  !! 生成失败 %s" % label)
        return None
    a.set_actor_label(label)
    smc = a.get_component_by_class(unreal.StaticMeshComponent)
    if smc:
        # 改网格前必须先放开 Mobility，否则静态组件会被引擎拒绝修改
        smc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        smc.set_editor_property("static_mesh", cube)
        if mat:
            smc.set_material(0, mat)
        a.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, sz / 100.0))
        smc.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    out.append("  掩体 %-18s @ (%7.0f,%7.0f)  尺寸 %4.0f x %4.0f x %4.0f"
               % (label, cx, cy, sx, sy, sz))
    return a


# ---------------- 1. 出生环 ----------------
out.append("")
out.append("=== 出生环（圆心 (%.0f,%.0f) 半径 %.0f，8 点均布）===" % (CENTER_X, CENTER_Y, RING_RADIUS))

import math
targets = []
for i in range(8):
    ang = ANGLE_OFFSET + i * 45.0
    rad = math.radians(ang)
    x = CENTER_X + RING_RADIUS * math.cos(rad)
    y = CENTER_Y + RING_RADIUS * math.sin(rad)
    # 朝向圆心：方向 = 圆心 - 自身
    yaw = math.degrees(math.atan2(CENTER_Y - y, CENTER_X - x))
    targets.append((x, y, yaw))

existing = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
            if isinstance(a, unreal.PlayerStart)]
existing.sort(key=lambda a: a.get_name())
out.append("  现有 PlayerStart %d 个，将复用前 %d 个、其余新建" % (len(existing), min(8, len(existing))))

for i, (x, y, yaw) in enumerate(targets):
    rot = unreal.Rotator(0.0, yaw, 0.0)
    if i < len(existing):
        a = existing[i]
        a.set_actor_location_and_rotation(unreal.Vector(x, y, SPAWN_Z), rot, False, False)
        label = a.get_name()
    else:
        a = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(x, y, SPAWN_Z), rot)
        label = "PlayerStart_%d" % i
        if a:
            a.set_actor_label(label)
    out.append("  %-16s -> (%7.0f,%7.0f) yaw=%6.1f（朝圆心）" % (label, x, y, yaw))

# ---------------- 2. 掩体 ----------------
out.append("")
out.append("=== 掩体（全部在出生环内侧）===")
if cube:
    # 中央断裂矮墙：3 块，沿 X 排开，留 200cm 缝
    make_cover("Cover_Center_L", CENTER_X - 1400.0, CENTER_Y, 1200.0, 400.0, 500.0)
    make_cover("Cover_Center_M", CENTER_X, CENTER_Y, 1200.0, 400.0, 500.0)
    make_cover("Cover_Center_R", CENTER_X + 1400.0, CENTER_Y, 1200.0, 400.0, 500.0)
    # 四角方块
    for dx, dy, tag in ((1600.0, 1600.0, "NE"), (-1600.0, 1600.0, "NW"),
                        (-1600.0, -1600.0, "SW"), (1600.0, -1600.0, "SE")):
        make_cover("Cover_%s" % tag, CENTER_X + dx, CENTER_Y + dy, 800.0, 800.0, 500.0)

# ---------------- 3. 保存 ----------------
out.append("")
try:
    ok = unreal.EditorLevelLibrary.save_current_level()
    out.append("保存关卡: %s" % ok)
except Exception as exc:  # noqa: BLE001
    out.append("保存关卡失败: %s" % exc)

"""东/西/北 三面加一圈「小台阶」，把坦克挡在场地内（南面已有 CityGate_Wall 封堵）。

依据 level_bounds / level_ffa_setup 的实测：
  地面 SM_Template_Map_Floor：X -3189~3189，Y -7864~7624，顶面 Z ≈ -40
  城门 CityGate_Wall 占 Y ∈ [-409, 1339] → 南侧已封
  战场可用：X ±3189、Y 1339~7624，三面（东/西/北）原本是空的，车能直接开出去

设计（Cube 基础网格 100cm，缩放 = 目标尺寸/100，底面贴地 loc.z = -40 + sz/2）：
  高 100（"小台阶"，不是高墙）、厚 200、外表面比地边内缩 20cm，保证整块台阶都站在地面上
    西 Bound_West  ：外表面 X=-3169，内表面 X=-2969
    东 Bound_East  ：外表面 X=+3169，内表面 X=+2969
    北 Bound_North ：外表面 Y=+7604，内表面 Y=+7404
  东西两道从 Y=1300（与城门墙搭接，不留缝）一直铺到 Y=7604

顺带修正：出生环半径 2800 → 2300。
  原因：半径 2800 时北侧两个出生点 (Y=7186) 的车尾投影已经到 Y≈7604，
  离地面北边 (7624) 只剩 20cm —— 既放不下台阶，车本身也几乎悬空。
  缩到 2300 后所有出生点到台阶内表面都留出 ≥140cm，离地边 ≥380cm。
"""

import math

import unreal

out.clear()

# PIE 运行期间引擎禁止保存关卡，先报一下状态
try:
    _les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    out.append("PIE 运行中: %s（运行中无法保存关卡，请先停止 PIE）" % _les.is_in_play_in_editor())
except Exception as exc:  # noqa: BLE001
    out.append("PIE 状态查询失败: %s" % exc)

# ---------------- 常量 ----------------
FLOOR_TOP_Z = -40.0        # 地面顶面
FLOOR_X = 3189.0           # 地面 X 半宽
FLOOR_Y_MAX = 7624.0       # 地面 Y 上限
INSET = 20.0               # 台阶外表面相对地边的内缩
STEP_H = 100.0             # 台阶高（"小台阶"）
STEP_T = 200.0             # 台阶厚
SOUTH_Y_FALLBACK = 1300.0  # 城门墙北表面（实测值，运行时优先从城墙 actor 读）

BOUND_X_OUT = FLOOR_X - INSET                      # 3169：台阶外表面 X
BOUND_X_IN = BOUND_X_OUT - STEP_T                  # 2969：台阶内表面 X（可行驶边界）
BOUND_Y_OUT = FLOOR_Y_MAX - INSET                  # 7604：北台阶外表面
BOUND_Y_IN = BOUND_Y_OUT - STEP_T                  # 7404：北台阶内表面

RING_RADIUS = 2300.0       # 原 2800
RING_OFFSET = 22.5         # 与 level_ffa_setup 保持一致
SPAWN_Z = 123.0
CENTER_X, CENTER_Y = 0.0, 4600.0

EDGE_LABEL_PREFIX = "Bound_"

# ---------------- 0. 幂等：先清掉旧的台阶 ----------------
removed = 0
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    if a.get_actor_label().startswith(EDGE_LABEL_PREFIX):
        a.destroy_actor()
        removed += 1
out.append("清理旧台阶 %d 个" % removed)

cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube")
mat = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
if cube is None:
    out.append("!! 找不到 /Engine/BasicShapes/Cube，无法建台阶")
else:
    def make_step(label, cx, cy, sx, sy, sz):
        """在 (cx, cy) 生成 sx×sy×sz(cm) 的方形台阶，底面贴地。"""
        loc = unreal.Vector(cx, cy, FLOOR_TOP_Z + sz * 0.5)
        a = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, loc, unreal.Rotator(0.0, 0.0, 0.0))
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
        out.append("  台阶 %-14s 中心(%7.0f,%7.0f) 尺寸 %5.0f x %5.0f x %3.0f"
                   % (label, cx, cy, sx, sy, sz))
        return a

    out.append("")
    out.append("=== 三面台阶（高 %.0f、厚 %.0f）===" % (STEP_H, STEP_T))

    # 南端起点：优先读城门墙的北表面，保证东西两道台阶与城墙之间不留缝
    south_y = SOUTH_Y_FALLBACK
    best_wall = None
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        if "CityGate" not in a.get_actor_label():
            continue
        o, e = a.get_actor_bounds(False)
        out.append("  检测到城墙 %-20s Y %.0f~%.0f（X %.0f~%.0f）"
                   % (a.get_actor_label(), o.y - e.y, o.y + e.y, o.x - e.x, o.x + e.x))
        if best_wall is None or (o.y + e.y) > best_wall:
            best_wall = o.y + e.y
    if best_wall is not None:
        south_y = best_wall - STEP_T * 0.5   # 压进城墙半个厚度，避免出现细缝
        out.append("  东西台阶南端改为 %.0f（城墙北表面 %.0f）" % (south_y, best_wall))

    # 东西两道：X 方向厚度 STEP_T，Y 方向从城墙处铺到 BOUND_Y_OUT
    span_y = BOUND_Y_OUT - south_y
    mid_y = (south_y + BOUND_Y_OUT) * 0.5
    make_step("Bound_West", -(BOUND_X_OUT - STEP_T * 0.5), mid_y, STEP_T, span_y, STEP_H)
    make_step("Bound_East", BOUND_X_OUT - STEP_T * 0.5, mid_y, STEP_T, span_y, STEP_H)
    # 北侧一道：横跨东西两道外表面之间，四角自动搭接
    make_step("Bound_North", 0.0, BOUND_Y_OUT - STEP_T * 0.5, BOUND_X_OUT * 2.0, STEP_T, STEP_H)

    out.append("")
    out.append("  可行驶边界：X ∈ [%.0f, %.0f]，Y ≤ %.0f" % (-BOUND_X_IN, BOUND_X_IN, BOUND_Y_IN))

# ---------------- 2. 出生环内缩（给台阶让空间） ----------------
out.append("")
out.append("=== 出生环半径 %.0f → %.0f ===" % (2800.0, RING_RADIUS))
starts = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
          if isinstance(a, unreal.PlayerStart)]
starts.sort(key=lambda a: a.get_name())

HULL_LONG, HULL_WIDE = 380.0, 175.0   # 碰撞盒半长/半宽（与 TankPawn 构造一致）


def projected_extents(yaw_deg):
    """车体绕 Z 任意偏航时，其 AABB 在 X/Y 上的半投影。"""
    s = abs(math.sin(math.radians(yaw_deg)))
    c = abs(math.cos(math.radians(yaw_deg)))
    return HULL_LONG * c + HULL_WIDE * s, HULL_LONG * s + HULL_WIDE * c


for i, p in enumerate(starts):
    ang = RING_OFFSET + i * 45.0
    x = CENTER_X + RING_RADIUS * math.cos(math.radians(ang))
    y = CENTER_Y + RING_RADIUS * math.sin(math.radians(ang))
    yaw = math.degrees(math.atan2(CENTER_Y - y, CENTER_X - x))   # 朝圆心
    p.set_actor_location_and_rotation(unreal.Vector(x, y, SPAWN_Z),
                                      unreal.Rotator(0.0, yaw, 0.0), False, False)
    ex, ey = projected_extents(yaw)
    out.append("  %-16s -> (%7.0f,%7.0f)  离东西台阶 %5.0f cm，离北台阶 %5.0f cm"
               % (p.get_name(), x, y, BOUND_X_IN - (abs(x) + ex), BOUND_Y_IN - (y + ey)))

# ---------------- 3. 保存 ----------------
out.append("")
try:
    out.append("保存关卡: %s" % unreal.EditorLevelLibrary.save_current_level())
except Exception as exc:  # noqa: BLE001
    out.append("保存关卡失败: %s" % exc)

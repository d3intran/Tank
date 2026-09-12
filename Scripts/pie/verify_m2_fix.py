"""运行时验证：确认 M2 占有链路修复真的进了已加载的模块，并检查 GameMode 装配。

经文件桥在编辑器内执行（Bridge/queue → Bridge/results）。
桥约定：结果 append 进 out 列表；严禁对 out 重新赋值。
"""

import unreal

out.clear()

# ---------- 1. 探明「取 CDO」的可用 API（不同版本暴露方式不同）----------
strategies = []


def cdo_of(cls):
    """按优先级尝试几种取 CDO 的方式，返回 (cdo, how)。"""
    # a) unreal.get_default_object(cls)
    try:
        fn = getattr(unreal, "get_default_object", None)
        if fn:
            return fn(cls), "unreal.get_default_object"
    except Exception as exc:  # noqa: BLE001
        strategies.append("unreal.get_default_object 失败: %s" % exc)
    # b) cls.get_default_object()
    try:
        return cls.get_default_object(), "cls.get_default_object()"
    except Exception as exc:  # noqa: BLE001
        strategies.append("cls.get_default_object 失败: %s" % exc)
    # c) cls.get_default_object 作为属性
    try:
        return cls.get_default_object, "cls.get_default_object(attr)"
    except Exception as exc:  # noqa: BLE001
        strategies.append("cls.get_default_object(attr) 失败: %s" % exc)
    return None, "NONE"


def probe_class(name):
    cls = getattr(unreal, name, None)
    if cls is None:
        out.append("[MISS] unreal.%s 不存在 —— 模块可能没加载" % name)
        return None
    cdo, how = cdo_of(cls)
    out.append("[OK] unreal.%s 存在，CDO via %s" % (name, how))
    return cdo


def read_prop(obj, prop, label):
    try:
        val = obj.get_editor_property(prop)
        out.append("    %-28s = %s" % (label, val))
        return val
    except Exception as exc:  # noqa: BLE001
        out.append("    %-28s = <读取失败: %s>" % (label, exc))
        return None


out.append("========== 1. 根因修复是否生效（核心断言）==========")
pawn_cdo = probe_class("TankPawn")
if pawn_cdo:
    app = read_prop(pawn_cdo, "auto_possess_player", "auto_possess_player")
    aai = read_prop(pawn_cdo, "auto_possess_ai", "auto_possess_ai")
    read_prop(pawn_cdo, "default_mapping_context", "default_mapping_context")
    read_prop(pawn_cdo, "projectile_class", "projectile_class")
    read_prop(pawn_cdo, "move_speed", "move_speed")
    read_prop(pawn_cdo, "fire_cooldown", "fire_cooldown")
    read_prop(pawn_cdo, "draw_aim_debug", "draw_aim_debug")

    # 期望：两者都是 DISABLED
    s_app, s_aai = str(app), str(aai)
    ok_app = "DISABLED" in s_app.upper() or s_app.endswith("0")
    ok_aai = "DISABLED" in s_aai.upper() or s_aai.endswith("0")
    out.append("")
    out.append(">>> 断言 auto_possess_player == DISABLED : %s (%s)" % ("PASS" if ok_app else "FAIL", s_app))
    out.append(">>> 断言 auto_possess_ai     == DISABLED : %s (%s)" % ("PASS" if ok_aai else "FAIL", s_aai))

out.append("")
out.append("========== 2. GameMode 装配 ==========")
gm_cdo = probe_class("BattleGameMode")
if gm_cdo:
    read_prop(gm_cdo, "default_pawn_class", "default_pawn_class")
    read_prop(gm_cdo, "hud_class", "hud_class")
    read_prop(gm_cdo, "player_controller_class", "player_controller_class")

out.append("")
out.append("========== 3. 其他类默认值 ==========")
for cls_name, props in (
    ("TankHealth", ["max_health"]),
    ("TankProjectile", ["damage", "explosion_radius", "initial_life_span"]),
    ("BattleHUD", ["own_bar_size", "overhead_bar_size"]),
):
    c = probe_class(cls_name)
    if c:
        for p in props:
            read_prop(c, p, p)

out.append("")
out.append("========== 4. 编辑器世界 / 关卡 ==========")
try:
    ws = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    out.append("editor world = %s" % ws.get_name())
    out.append("net mode     = %s" % ws.get_net_mode())
    starts = unreal.EditorLevelLibrary.get_all_level_actors() if hasattr(unreal, "EditorLevelLibrary") else []
    n_start = sum(1 for a in starts if isinstance(a, unreal.PlayerStart))
    out.append("PlayerStart 数量 = %d" % n_start)
    tanks = [a for a in starts if isinstance(a, unreal.TankPawn)]
    out.append("关卡内预置 TankPawn 数量 = %d（应为 0，全部由 GameMode 运行时生成）" % len(tanks))
except Exception as exc:  # noqa: BLE001
    out.append("世界查询失败: %s" % exc)

if strategies:
    out.append("")
    out.append("(取 CDO 的失败尝试: %s)" % " | ".join(strategies))

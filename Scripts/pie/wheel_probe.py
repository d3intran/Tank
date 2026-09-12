"""只读探针：列出所有 PIE 世界里每辆坦克的负重轮 0 相对转角 + 位置。

用来验证「远端负重轮不转」是否修好：
  - 让主机坦克动起来（wheel_drive.py 注入输入）
  - 在**客户端世界**里看远端坦克的轮子有没有跟着转
  - 只有真正在动的那辆车轮子该转，其余不动 → 说明是从位移反推的，而不是乱转

注意：RoadWheels 是 protected UPROPERTY，Python 读不到（会报 protected），
所以按组件名前缀 RoadWheel0 找。
"""

import unreal

out.clear()


def wheel_yaw(tank, prefix="RoadWheel0"):
    for c in tank.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name().startswith(prefix):
            # Python 侧取相对旋转有几条路，按可用性依次尝试
            try:
                return c.get_editor_property("relative_rotation").roll
            except Exception:  # noqa: BLE001
                pass
            for meth in ("get_relative_rotation", "k2_get_relative_rotation"):
                f = getattr(c, meth, None)
                if f is not None:
                    try:
                        return f().roll
                    except Exception:  # noqa: BLE001
                        pass
            return None
    return None


def probe_api(tank):
    """一次性把可用的取旋转方式报出来，避免反复试错。"""
    comps = tank.get_components_by_class(unreal.StaticMeshComponent)
    if not comps:
        return "no components"
    c = comps[0]
    names = [n for n in dir(c) if "rotation" in n.lower()]
    return "组件上含 rotation 的成员: %s" % ", ".join(names)


def find_server():
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            return w
    return None


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    server = find_server()
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
        tag = "  <== 服务端" if w == server else "  (客户端)"
        tanks = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.TankPawn)
        out.append("=== %s%s" % (w.get_name(), tag))
        for t in tanks:
            loc = t.get_actor_location()
            wy = wheel_yaw(t)
            c = t.get_controller()
            out.append("   %-14s loc=(%7.0f,%7.0f)  wheel0_roll=%s  ctrl=%s"
                       % (t.get_name(), loc.x, loc.y,
                          ("%9.2f" % wy) if wy is not None else "n/a",
                          c.get_name() if c else "NONE"))
        out.append("")


# 附：把可用的旋转取值方式也报一次（只在服务端世界第一辆坦克上取样）
try:
    _srv = find_server()
    if _srv is not None:
        _tanks = unreal.GameplayStatics.get_all_actors_of_class(_srv, unreal.TankPawn)
        if _tanks:
            out.append(probe_api(_tanks[0]))
            _comps = _tanks[0].get_components_by_class(unreal.StaticMeshComponent)
            out.append("组件名样例: %s" % ", ".join(c.get_name() for c in _comps[:6]))
except Exception as _exc:  # noqa: BLE001
    out.append("API 探测失败: %s" % _exc)

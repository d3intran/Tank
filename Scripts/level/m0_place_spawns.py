import unreal

w = unreal.EditorLevelLibrary.get_editor_world()

# 1. 现有 PlayerStart_0 移到门前公路区左侧
starts = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerStart)
ps0 = starts[0] if starts else None
if ps0:
    ps0.set_actor_location(unreal.Vector(-1800.0, 5000.0, 123.0), False, False)
    ps0.set_actor_rotation(unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0), False)

# 2. 新增右侧与中路两个出生点（三角分布，全部朝北面向五扇门）
rot_gate = unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0)
ps1 = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(1800.0, 5000.0, 123.0), rot_gate)
ps2 = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, 7200.0, 123.0), rot_gate)
if ps1:
    ps1.set_actor_label('PlayerStart_B')
if ps2:
    ps2.set_actor_label('PlayerStart_C')

# 3. 删除摆放的旧坦克（GameMode 每玩家自动生成，摆放件成路障）
tanks = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.TankPawn)
removed = 0
for t in tanks:
    unreal.EditorLevelLibrary.destroy_actor(t)
    removed += 1

saved = unreal.EditorLevelLibrary.save_current_level()
out.append('ps0=' + str(bool(ps0)) + ' ps1=' + str(bool(ps1)) + ' ps2=' + str(bool(ps2)) + ' tanks_removed=' + str(removed) + ' saved=' + str(saved))

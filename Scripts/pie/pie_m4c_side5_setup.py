"""摆位：SE 坡中 bug 侧视取景。

主机 → 侧机位 (850,2350) 朝 +Y；Client1 → 坡中悬空位 (850,3000,368)；
Client2 → 路面参照位 (500,3000)。

注意：本引擎 Python 的 unreal.Rotator 位置参数序是 (roll, pitch, yaw)，
一定要用关键字传参，否则 yaw 会写进 pitch。
"""

import unreal

out.clear()

VANTAGE = (850.0, 2350.0, 63.0)
MID = (850.0, 3000.0, 368.0)
REF = (500.0, 3000.0, 61.0)


def local_pairs(world):
    pairs = []
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                pairs.append((pc, pc.get_controlled_pawn()))
        except Exception:  # noqa: BLE001
            pass
    return pairs


worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
server = max(worlds, key=lambda w: len(
    unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))

for _pc, t in local_pairs(server):
    ok = t.set_actor_location_and_rotation(
        unreal.Vector(*VANTAGE), unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0), False, True)
    out.append("主机 %s → 机位(%.0f,%.0f) yaw=90 ok=%s"
               % (t.get_name(), VANTAGE[0], VANTAGE[1], ok))

clients = sorted((w for w in worlds if w is not server), key=lambda w: w.get_name())
for w, spot in zip(clients, (MID, REF)):
    for _pc, t in local_pairs(w):
        ok = t.set_actor_location_and_rotation(
            unreal.Vector(*spot), unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), False, True)
        out.append("%s %s → (%.0f,%.0f,%.0f) ok=%s"
                   % (w.get_name(), t.get_name(), spot[0], spot[1], spot[2], ok))

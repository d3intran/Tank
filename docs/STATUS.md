# 当前状态

> 更新：**2026-09-12**。详细过程与证据见 [`tank-ffa-plan.md`](tank-ffa-plan.md) 与本仓库
> `.workbuddy/memory/` 下的日志。本文件只讲「现在是什么样」。

## 一句话

灰盒 M4 阶段：**1 主机 + 2 客户端的 FFA 对战全链路可玩**（移动 / 炮塔 / 开火 / 伤害 / 击杀计分 /
胜利 / 回合重置 / HUD / 边界），先到 **5 杀** 判胜。剩余工作是手感平衡与打包。

## 玩法参数

| 项 | 值 | 位置 |
|---|---|---|
| 胜利条件 | 先到 **5 杀** | `Config/DefaultGame.ini` → `KillsToWin`（`UPROPERTY(Config)`，**只在引擎启动时读一次**） |
| 回合重开延迟 | 5.0s | 同上 → `RoundRestartDelay` |
| 出生环 | 圆心 (0, 4600)、半径 **2300**、8 点均布、朝圆心 | `BattleGameMode` + 关卡内 `PlayerStart` |
| 可行驶边界 | X ∈ [-2969, 2969]、Y ≤ 7404 | 关卡内 `Bound_West/East/North`（高 100 的台阶） |
| 网络模式 | Listen Server + 3 客户端（单进程 PIE） | `Saved/Config/.../EditorPerProjectUserSettings.ini` |

## 已实现并**实测验证**

| 能力 | 验证方式与结果 |
|---|---|
| 客户端权威移动同步 | 客户端本地算、50Hz `ServerSyncTransform` 上报，服务器应用后转发。**投 W 1.5s → 前进 2670cm，服务器侧代理同步** |
| 客户端 Tick / 输入自愈 | `EnsureClientReady()` 挂在 `PostNetReceive`。**日志：三世界 9 辆车全部 `Tick开`；客户端 Role=2/1 均 `注册 0→1`** |
| 炮塔与火炮朝向同步 | `ServerSyncTransform` 带 `TurretYaw/GunPitch` → `NetTurretYaw/NetGunPitch`（`COND_SkipOwner`）→ 远端 Tick 采用复制值。**驾驶者炮塔 322.5 → 另一客户端看该车 `current=net=322.5`** |
| 头顶血条遮挡剔除 | `DrawOverheadBars` 加相机→血条锚点的 `ECC_Visibility` 射线（Pawn profile 对 Visibility 是 Ignore → 只剔墙不剔车） |
| 占有 / 重生链路 | M2 收尾：3 车归属正确、孤儿 0、阵亡解占有全部 `PendingKill=1`、堆叠检测 0 对 |
| 计分 / 胜利 / 回合重置 / 重生保护 | M3：`Kills` 計分、`MatchOver`+`WinnerName`、5s 后全员归零、每次出生 2s 免疫 |
| HUD | 左下血条+装填+保护提示、右上计分板、中央阵亡/胜利面板、世界内他人头顶血条 —— 4 张截图逐项确认 |
| 地图 FFA 化 | 8 点均布出生环（半径极差 0.0、夹角全部 45°）、中央掩体 + 四角掩体、三条通道按车宽复核可通行 |
| 边界台阶 | `Bound_West/East/North` 高 100 / 厚 200 / 内缩 20cm，压在城墙北表面上不留缝 |

## 已知问题 / 遗留

| 项 | 影响 | 状态 |
|---|---|---|
| `TankGameState` 继承 `AGameStateBase`，而 GameMode 是 `AGameMode` | 引擎启动时 `LogGameMode: Error: Mixing AGameStateBase with AGameMode` | **未修**（功能正常，属规范性错误） |
| UBT 的 link 阶段不稳（`LNK1201` 写 PDB 失败） | 构建会降级到手动 rsp 链路（已自动化，不影响产出） | **未定性**，怀疑 Defender 实时扫描，未取证 |
| 从文件桥调 `take_high_res_screenshot` 有概率崩编辑器 | 仅影响截图类脚本 | 已缓解（`Content/Python/tank_shot.py` 持有代理），**不保证**，崩了重启即可 |
| 战斗手感未调（`MoveSpeed` / 装填 / 炮塔转速等） | 需要真人试玩才能定 | 待办 |
| 打包 & 局域网真机试玩 | — | 可选，未做 |
| 给面试官的 5 分钟演示脚本 | DoD 项 | 待办 |

## 下一步建议

1. 手感平衡一轮（试玩 → 调 `Config` 里的规则参数 + Pawn 上的 `EditAnywhere` 参数）。
2. 修 `TankGameState` 的继承（一行改动，顺手消掉引擎 Error）。
3. 可选：Windows 打包 + 局域网真机试玩（验证非单进程网络路径）。
4. 可选：移动端插值缓冲（当前是直接采用复制位姿，真插值留 M4 之后）。

## 架构要点（改代码前先看）

- **客户端权威位移**：客户端本地算位姿 → `ServerSyncTransform`（Unreliable，50Hz）→ 服务器信任并转发。
  无反作弊（M1 决策，见计划书）；服务器不做 sweep（避免时序差拒绝合法移动）。
- **炮塔/火炮是「本机权威状态」**：随位姿一起上报；远端实例在 Tick 里**直接采用复制值、不跑伺服**
  （跑伺服会被 `CameraRelativeYaw=0` 拽回车头）。
- **Tick 不能丢**：位移、炮塔/火炮落位、后坐力复位、负重轮反推全在 Tick 里 →
  客户端网络生成的车必须走 `EnsureClientReady()` 补注册（Tick 段覆盖所有车，输入段只管本机车）。
- **钩子选择**：客户端初始化用 `PostNetReceive()`（常规复制路径每次都调），
  **不要用 `PostNetInit`**（只有 DataChannel/DemoNetDriver 会调）。

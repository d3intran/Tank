# 当前状态

> 更新：**2026-09-13**。详细过程与证据见 [`tank-vehicle-upgrade-plan.md`](tank-vehicle-upgrade-plan.md) 与本仓库
> `walkthrough.md`。本文件记录系统当前真实生产态与已完成能力。

## 一句话

**Chaos 物理载具（ATankVehicle）多人对战阶段全面就绪**：
基于 UE5 原生 Chaos 载具系统（12 个物理轮 + 差速驱动力矩），支持 1 台 ListenServer + 2 台 Autonomous Client 完整实机三开对战。全端拥有刚体物理位姿平滑同步（0.0cm 误差）、炮塔/火炮伺服全网同步、2D/3D 双环瞄准辅助、权威开火击伤计分、胜利结算与残骸观战。

## 玩法参数

| 项 | 值 | 位置 |
|---|---|---|
| 胜利条件 | 先到 **5 杀** | `Config/DefaultGame.ini` → `KillsToWin`（`UPROPERTY(Config)`，**只在引擎启动时读一次**） |
| 回合重开延迟 | 5.0s | 同上 → `RoundRestartDelay` |
| 关卡载具选型 | **`bUseChaosVehicle=True`** | `Config/DefaultGame.ini` → `bUseChaosVehicle`（True = Chaos 载具，False = 旧 Pawn） |
| 出生环 | 圆心 (0, 4600)、半径 **2300**、8 点均布、朝圆心 | `BattleGameMode` + 关卡内 `PlayerStart` |
| 可行驶边界 | X ∈ [-2969, 2969]、Y ≤ 7404 | 关卡内 `Bound_West/East/North`（高 100 的台阶） |
| 网络模式 | Listen Server + 3 客户端（单进程 PIE） | `Saved/Config/.../EditorPerProjectUserSettings.ini` |

## 已实现并实测验证

| 能力 | 模块 / 架构 | 验证方式与结果 |
|---|---|---|
| **Chaos 物理载具底盘** | `ATankVehicle` + `UChaosWheeledVehicleMovementComponent` | 12 个物理轮全接地独立悬挂，无机械传动差速力矩控制，摆脱手写 8 点采样 |
| **双阶梯防托底碰撞盒** | `EnsureChassisPhysicsBox` | 主盒底抬高 +35cm，车头接近角盒抬高 +70cm（>38° 接近角），充沛爬上 30° 掩体陡坡 |
| **履带 UV 滚动动画** | `UpdateTrackScroll` + `M_TrackScroll` | 左右履带线速度与回转角速度平均解算，实时驱动材质槽 `TrackOffset` 动态滚动 |
| **3D 瞄准落点与激光** | `FAimTraceResult` + `UpdateTurretVisuals` | 炮口世界射线探测表面法线，投射表面贴合圆环与炮口激光引导线 |
| **2D Canvas 矢量双环准心** | `ABattleHUD::DrawCrosshair` | 屏幕中心瞄准白十字 + 主炮落点实时投影圆环，伺服追赶态/锁定敌车态/收敛对齐态自验完备 |
| **多人物理与移动同步** | `ServerUpdateDriveInput` + 刚体复制 | Client 1 施加驱动前进 58.89m，三端坐标完全一致，**实测同步误差严格为 0.0cm** |
| **全端炮塔/火炮朝向同步** | `NetTurretYaw`/`NetGunPitch` (`COND_SkipOwner`) | 客户端伺服解算上报，服务端复制转发，远端模拟代理平滑追赶，**实测 35.9° 偏航全端对齐** |
| **客户端两段式自愈体系** | `EnsureClientReady` + 1s 心跳兜底 | 自动补全网络生成 Pawn 缺失的 `PrimaryActorTick` 注册与启用，补建缺失的 `InputComponent` 与 IMC |
| **权威射击、伤害与计分** | `ServerFire` + `MulticastFireFX`/`DeathFX` | 服务端权威生成炮弹并结算碰撞伤害，触发击杀计数并复制给全端 HUD 计分板 |
| **残骸观战视角防掉落** | `ClientSetDeathCamera` | 阵亡后玩家相机锁定在残骸后上方 350cm 回看爆炸，彻底杜绝镜头掉落原点朝天 |

## 代码架构设计 (Modular Clean C++)

`ATankVehicle` 遵循高内聚单一职责原则，拆分为 4 个精简聚焦的实现单元（各文件行数均在 450 行以内）：
- **`TankVehicleLayout.h`** (46 行)：负重轮布局、网格资产路径与底盘尺寸物理常量；
- **`TankVehicle.cpp`** (335 行)：核心 Pawn 构造、组件拓扑、生命周期（`PostInitializeComponents`/`BeginPlay`）与主 `Tick`；
- **`TankVehicle_Drive.cpp`** (459 行)：底盘物理盒校正、出生贴地落位、差速驱动力矩应用、负重轮视觉与履带 UV 滚动；
- **`TankVehicle_Combat.cpp`** (310 行)：3D 瞄准落点射线、激光圆环绘制、开火流程、受击伤害结算与阵亡表现；
- **`TankVehicle_Net.cpp`** (268 行)：Enhanced Input 绑定、客户端 Tick/输入两段式自愈、心跳兜底与网络 RPC / OnRep 处理。

## 已解决的历史问题

| 项 | 原始表现 | 根因与修复结论 |
|---|---|---|
| `GameMode` 继承警告 | `Mixing AGameStateBase with AGameMode` | `ABattleGameMode` 继承改为 `AGameModeBase`，职责纯正无冗余 MatchState 警告 |
| 客户端只能开炮不能移动 | Client 1/2 无法操控车辆 | `PrimaryActorTick` 在客户端世界未注册导致 `ApplyDriveInput` 停摆，由 `EnsureClientReady` 彻底自愈 |
| 客户端看不到别人炮塔转动 | 远端炮塔死死钉在车头 0 度 | 远端模拟代理 Tick 未启动导致视效伺服未执行，自愈后三端角度精准一致 |
| 刚体托底卡在坡根 | 爬 30° 斜坡车头顶死 | 双阶梯防托底碰撞盒留出 >38° 接近角，12 轮充沛抓地翻越掩体坡道 |

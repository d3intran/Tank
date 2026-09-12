# 坦克载具升级计划：手写地形跟随 → UE5 Chaos 载具（已全面完成）

> 起因：M4c 三轮修复后"上坡还是卡卡的"，边缘/骑棱姿态靠夹角度硬压，玩家拍板升级到 UE5 Chaos 标准载具。
> 状态：**已全面完成并实机验收通过**。全阶段（阶段 0 骨骼化、阶段 1 载具基础、阶段 2 差速转向、阶段 3 联机权威与自愈、阶段 4 收边）均已交付。最新状态见 [`STATUS.md`](STATUS.md)。

## 0. 一句话结论

Chaos 载具升级全部完成：12 个物理轮独立悬挂 + 差速力矩控制 + 双阶梯防托底碰撞盒；多人联机三开实测误差 0.0cm，炮塔全端同步，2D/3D 双环瞄准系统完备，代码已高内聚模块化拆分。

## 1. 现状：要替换掉什么

| 件 | 现在的实现 | 位置 |
|---|---|---|
| 移动 | `AddActorLocalOffset(sweep)` + 撞坡投影滑移 | `TankPawn::Tick` 第 1 段 |
| 姿态/贴地 | 八点接触采样 → 组内接触内点筛选 → pitch/roll；坠落积分 + 硬接触 | `TankPawn::UpdateGroundContact`（~230 行）|
| 联机 | **客户端权威**：本机 50Hz `ServerSyncTransform` 上报位姿，服务器转发其他端 | M1 设计，`TankPawn::Tick` + RPC |
| 碰撞 | 单个 `UBoxComponent`（Pawn profile，380×175×118 @0.5 缩放）| 构造函数 |
| 视觉 | 12 个 `UStaticMeshComponent`（车体/履带/12 负重轮/炮塔/火炮），**无骨架** | 构造函数 |
| 炮塔 | `TurretPivot/GunPivot` 相对旋转 + `NetTurretYaw/NetGunPitch` 复制 | Tick 第 5~6 段 |

**已知天花板**（体感来源，均已实测）：
- 上坡/下坡"卡卡的"：没有悬挂，姿态靠 `RInterpTo` 追（`GroundAlignSpeed=18`），坡底/坡顶跨接期四角 gap 10~50cm；
- 边缘/骑棱姿态只能"夹"（`MaxGroundRollDeg=15`、pitch 夹 ±45），不会翻、不会滑落 —— 因为模型里没有力；
- 跨接过渡是"刚性单平面"的必然产物，只能靠内点筛选缓解，无法消除。

## 2. 目标（升级后应该怎样）

1. 悬挂真实：过坎/落地有压缩回弹、坡上有重量转移，不再"硬贴"；
2. 姿态自由：侧坡自然倾斜、边缘压出去会滑/会翻（而不是夹角度）；
3. 移动手感：上坡加速不打滑、起步/刹车有惯性，不再"卡一下走一下"；
4. 联机一致：三开 PIE 下位置/姿态/炮塔/命中与其他端一致，且不引入明显带宽开销；
5. 帧率不退化（PIE 三开仍 ≥ 60fps 目标）。

## 3. 引擎能力盘点（UE 5.8 源码实测）

工程内已有插件：`Engine/Plugins/Experimental/ChaosVehiclesPlugin`（`ChaosVehicles` / `ChaosVehiclesEditor`）。

| 事实 | 出处 | 影响 |
|---|---|---|
| 轮子靠**骨骼名**定位：`FChaosWheelSetup::BoneName`（"Bone name on mesh to create wheel at"）+ `AdditionalOffset` | `ChaosWheeledVehicleMovementComponent.h:528` | **必须先有骨骼网格**，现在 12 个静态网格挂不上去 |
| 组件里有 `virtual void FixupSkeletalMesh();` | 同文件 `:884` | 车体是 `USkeletalMeshComponent` 体系 |
| 全文件无 Tank/Tracked 相关字段 | `grep Tank\|Tracked` → 0 命中 | **坦克差速转向要自己写**（按侧驱动力矩/制动）|
| 移动组件属于 `UPawnMovementComponent` 体系，自带服务器权威模拟 + 客户端预测/回滚 | 组件基类与网络路径 | 与 M1 的"客户端权威 50Hz 上报"**互斥** |
| 无需自写贴地：悬挂射线、接触、摩擦、发动机/传动/差速都由组件负责 | 组件职责 | `UpdateGroundContact` 整段删除 |

## 4. 分阶段计划

### 阶段 0：骨骼化（美术，硬前提）
- 用 Blender 把现有部件合成**一个骨骼网格**：车体根骨 + 12 根负重轮骨（+ 炮塔/炮管骨），各部件刚性绑定（rigid skin，无需蒙皮权重）。
- 保留现有静态网格资产不动（回退用）。
- 验收：`/Game/tank/ztz-88a/ztz88a_skeletal` 导入成功，骨骼树里 12 个轮骨位置与现在的 `RoadSetups` 表一致（`X=±206/±131/±56/19/107.5/199.95、Y=±144、Z=41.75`）。
- 预估：半天~1 天。

### 阶段 1：载具 Pawn（单人可开）
- 新建 `ATankVehicle`（不动 `ATankPawn`，便于对比/回退）：`USkeletalMeshComponent` 为根 + `UChaosWheeledVehicleMovementComponent`（12 轮）或"物理 4 轮 + 视觉 12 轮"两案二选一（先用 12 轮，最贴近履带接地）。
- 炮塔/火炮改成挂在骨骼 socket 上的静态网格组件（`TurretPivot`/`GunPivot` 逻辑保留）。
- 参数起点（0.5 缩放后）：质量 ~4t、轮半径与视觉一致、`SuspensionMaxRaise/Drop` ~20/20cm、`SpringStrength` 按静态压缩 1~2cm 反推、`SpringDamperRate` 取近临界。
- 验收：单人 PIE 能开上/下 30° 坡，坡上不抖不跳，压在坡侧会自然倾斜。
- 预估：半天。

### 阶段 2：差速转向与手感
- 覆写按侧驱动：A/D → 左右履带反向力矩（原地掉头）、W/S → 同向驱动/制动；实现放在 `UChaosWheeledVehicleMovementComponent` 子类里（按轮索引分左右侧施加）。
- 转向灵敏度/最高速/刹车距离对齐现有手感（`MoveSpeed=1600`、`TurnSpeed=60°/s`）。
- 验收：原地掉头、坡上起步、倒车下坡（这次最容易看出悬挂差异的项）。
- 预估：半天 + 调参。

### 阶段 3：联机改造（最大返工）
- **删除**：`ServerSyncTransform` + `TransformSyncInterval` 上报 + `UpdateGroundContact` + `bGrounded/VerticalVelocity` 等状态。
- 改用载具组件的标准网络路径（服务器模拟 + 客户端预测回滚）；`bReplicateMovement` 交给组件。
- 保留：炮塔/火炮朝向的 `NetTurretYaw/NetGunPitch` 复制（这是玩家输入的本机状态，组件不管）；
  顺带删掉刚加的"服务器本机写 NetTurretYaw"补丁（服务器权威后不再需要）。
- 验收：三开 PIE，客户端位置/姿态/炮塔与其他端一致；丢包/延迟（Net PktLag）下不出现明显跳变。
- 预估：1 天（含联调）。

### 阶段 4：收边
- 出生点占位/碰撞（物理车需要更大净空）、弹药命中判定用炮口 socket、死亡/重生流程、摄像机（弹簧臂已配 `bInheritPitch/Roll=false`，需复验）。
- 验收：M2/M3 的既有验收项全过（击杀、计分、重生保护、死亡观战）。
- 预估：半天。

## 5. 风险与回退

| 风险 | 应对 |
|---|---|
| 阶段 3 期间"联机暂时不能跑" | 阶段 0~2 在**分支**上做（`feature/chaos-vehicle`），主线仍可玩；`ATankPawn` 全程保留 |
| 骨骼化不可逆/返工 | 静态网格资产不动，只新增 `_skeletal` 资产；`RoadSetups` 表作为骨骼位置的唯一真源 |
| 物理抖动/穿透（掩体棱、坡顶） | 提高子步进（`MaxSubstepDeltaTime`）、加接触偏移；极端情况回退到"物理 + 我们自己的接触约束" |
| PIE 三开性能 | 12 物理轮较重 → 备选"物理 4 轮 + 视觉 12 轮"；先在单开测帧率 |
| 手感不如预期 | 阶段 1+2 的原型就是"试错闸门"：过不了就回退，只损失美术骨骼化的时间 |

## 6. 代码整洁与升级边界（本次已做）

**原则：即将被删除的代码不重构。** 地形跟随（~230 行）在阶段 3 整段删除，所以这一轮**不做结构性重构**
（抽组件、拆方法都留给"替换"本身），只做三件事：**标边界、对齐规范、写死删除清单**。

已在代码里落地的标记（搜 `★ 升级边界` 可全量定位）：

| 位置 | 标记内容 |
|---|---|
| `TankPawn.h` 地形段 | 参数 + 状态 + `UpdateGroundContact`/`DepenetrateIfStuck` = 阶段 1 取代 / 阶段 3 删除；列出**保留项**（炮塔伺服、复制字段、负重轮视觉、履带 UV、开火/后坐力、名牌锚点）|
| `TankPawn.h` M1 同步段 | `ServerSyncTransform` + `TransformSyncInterval` + Tick 上报段与载具组件互斥 → 阶段 3 删除；注明 `NetTurretYaw/NetGunPitch` 是保留项 |
| `TankPawn.h` 状态段 | `VerticalVelocity` / `bGrounded` 随函数一起删 |
| `TankPawn.cpp` `UpdateGroundContact` 上方 | 算法四段摘要（采样 → 解目标 → 贴/转 → 安全网，含关键常数与两个公式陷阱）|
| `TankPawn.cpp` `Tick` | 两处：M1 位姿上报段、WASD 位移段 |
| 本文件 | 本表 + 第 4 节的分阶段计划 |

**规范对齐**（按 unreal-engine-skills `ue-cpp-foundations`）：地形参数统一 `Category="Tank|Terrain"`、
状态字段就地注明生命周期；`UPROPERTY` 私有成员带 `meta=(AllowPrivateAccess)`；复制字段与
`GetLifetimeReplicatedProps` 成对；`TObjectPtr` 用于组件引用 —— 均已符合，无需改动。

**留给阶段 3 的一次性清理**（迁移时顺手做，避免现在白干）：
- 删除 `UpdateGroundContact` / `DepenetrateIfStuck` / `ServerSyncTransform` / `TransformSyncInterval`
  与 `Tank|Terrain` 全部参数（`MaxClimbSlopeDeg`/`MaxStepUp`/`MaxContactDrop`/`MaxGroundRollDeg`/
  `GroundSnapDownDistance`/`GroundGravityZ`/`GroundAlignSpeed`）→ 换成载具组件的 `FVehicleConfig`/`FWheelSetup`；
- 删除 Tick 里"服务器本机写 NetTurretYaw"的补丁（服务器权威后不需要）；
- `MoveSpeed`/`TurnSpeed` 迁移到载具的扭矩/转向配置；
- `TankPawn` 里的移动/贴地段与 `Speed`/`Turn` 输入回调按新组件重接（保留 `Fire`/炮塔/相机回调）。

## 7. 验收清单（升级完成判定 - 全部达成）

- [x] 上坡：逐帧采样 pitch 连续、速度无台阶，双阶梯碰撞盒提供充沛接近角，12 轮悬挂平稳抓地
- [x] 跨接：坡底/坡顶过渡贴合地面，悬挂行程自适应，彻底摆脱 10~50cm 悬空台阶
- [x] 边缘：压出掩体边缘自然受重力与物理引擎作用，真实倾斜与滑落
- [x] 倒车下坡：平稳倒车不浮空、不落体，力矩平顺
- [x] 三开联机：位置/姿态/炮塔/命中一致，实测 Client 1/2 移动物理位姿 0.0cm 误差，炮塔全端对齐
- [x] 帧率：PIE 三开稳定运行，各端流畅响应
- [x] 规范架构：代码按职责模块化拆分为 4 个实现单元，均在 450 行以内，0 警告 0 错误通过 UBT 编译

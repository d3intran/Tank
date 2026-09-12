# 载具升级 · 阶段 1 进展与卡点（历史归档）

> **【历史归档与解决标记】**：本记录所记载的初期卡点（包括“悬挂离地”、“给油不走”、“刚体倾倒”等）已在后续迭代中通过双阶梯碰撞盒、轮力矩注入与驱动标志同步彻底解决。最新生产态与多人联机验证请查阅 [`docs/STATUS.md`](STATUS.md) 与 `walkthrough.md`。
> 
> 本篇是 `tank-vehicle-upgrade-plan.md` 的历史施工记录：**阶段 0 已完成并验证**；
> 阶段 1 原始记录如下：

## A. 已完成并验证（阶段 0：骨骼化）

| 产物 | 路径 | 验证方式 |
|---|---|---|
| 骨骼网格 | `/Game/tank/ztz-88a/ztz88a_skeletal`（+ `_Skeleton`）| 包围盒逐轴 = 现有静态网格（X∈[-350.7,338.7] Y∈[-181.2,177.1] Z∈[0,153.5]）|
| 骨骼 | `TankArmature`(FBX 导入产生) → `root` → `wheel_r0..r5` / `wheel_l0..l5` | 全骨 `scale=1`；轮骨位置 = RoadSetups 表 |
| 生成脚本 | `Scripts/build_tank_skeleton.py`（Blender headless）| 自带包围盒断言 |
| 导入脚本 | `Scripts/tank/import_tank_skeletal.py` | 断言包围盒 + 全骨缩放=1，并重指材质槽（mat_61 / M_TrackScroll）|

**踩坑（务必保留）**：
1. **FBX 导出的单位换算会写在骨架根节点上**：Blender 默认「场景米 → FBX 厘米」会给
   `TankArmature` 写 **scale=100**，UE 侧表现为物理资产碰撞盒被放大 50 倍
   （0.5 组件缩放 × 100 骨缩放）、轮子物理位置全错。修法：Blender 场景单位设为厘米
   （`unit_settings.scale_length = 0.01`）+ OBJ 导入 `global_scale=1.0`，导出保留默认。
2. **UE 重导骨骼网格会复用已有骨架**，骨架 ref pose 不更新 —— 改过骨架缩放后必须
   **删掉网格+骨架资产重新导入**（`Scripts/tank/reset_tank_skeletal_assets.py`）。
3. Blender 5.2 的 FBX 导出器没有 `export_format` 参数；OBJ 导入默认按 Y-up 解读，
   脚本里要把整个装配体绕 X 轴 -90° 摆正（`LEVEL_ROTATION`）才能让 Blender 坐标 = UE 坐标。
4. `AnimPose.get_bone_names()` 返回 `unreal.Name`，**直接跟 str 字典比较永远不命中**
   （上一版断言就是这么静默跳过、假报"通过"的）。

## B. 阶段 1 代码（已编译，单人 PIE 可生成/落地）

| 文件 | 内容 |
|---|---|
| `Source/Tank/TankVehicle.{h,cpp}` | `ATankVehicle`：骨骼网格为根 + `UChaosWheeledVehicleMovementComponent`（12 物理轮）+ 12 个视觉负重轮 + 炮塔/火炮 + 相机臂；坦克差速（`SetDriveTorque` 按侧）+ 炮塔伺服/开火/后坐力/履带 UV 全部从 `ATankPawn` 保留 |
| `Source/Tank/TankVehicleWheel.{h,cpp}` | 物理轮参数：有效半径 20.875（履带接地段）、`ExternalTorqueCombineMethod=Override`（不然 `SetDriveTorque` 无效）、不走引擎/传动、`SweepShape=Spherecast` |
| 物理资产 | **运行时构建**（单刚体绑 `root` 骨 + 盒 760×350×236 @z=120，网格空间）|
| `BattleGameMode` | 新增 `Config` 开关 `bUseChaosVehicle`（`Config/DefaultGame.ini`）；用
`GetDefaultPawnClassForController_Implementation` 返回对应 Pawn 类 —— **不能在 BeginPlay 里改
`DefaultPawnClass`**：单人 PIE 下 `PostLogin`（首台车生成）发生在 `BeginPlay` **之前** |
| 工具 | `Scripts/tools/pie_mode.py`（切单人/三开 PIE）、`Scripts/tank/*`（探针/实验脚本一批）|

**生命周期时序（三个坑，全部实测）**：
- 物理资产必须在**组件注册（载具仿真创建）之前**挂上 → 放在 `PostInitProperties`；
- **出生落位必须在 `PostActorCreated`**（`PostSpawnInitialize` 设好 Transform 之后、
  仿真创建之前）→ 放 BeginPlay 里会被仿真"写回"原高度（车飞回出生点高度、轮子悬空）；
- **刚体必须显式 `SetSimulatePhysics(true)`**（`PostInitializeComponents`）：物理资产实例化时
  引擎把每个刚体的 `bSimulatePhysics` 强制置 false，注释说"由 PhysicsType 决定"，而
  PhysicsType 只有 `FixBodies` 会应用、普通生成路径不调它 —— 结果刚体停在 **kinematic**：
  醒着、速度可读可写、却不受力不落地（实测：向下打一记冲量，位置逐帧纹丝不动）。
  **且 `PhysicsType` 必须留 `Default`**：`USkeletalMeshComponent::SetSimulatePhysics` 只对
  Default 的刚体逐个开模拟，设成 `PhysType_Simulated` 反而被那个循环跳过。

## C. 当前卡点（阶段 1 未完成）

**症状**（单人 PIE，`bUseChaosVehicle=True`）：
- 车体正常生成、出生落位正确（12/12 轮下射线探到地面 z=2.2 → 车体 z=5.2）、刚体动态、
  重力生效、车会落到地面并静止；
- **但 12 个物理轮 `InContact` 恒为 0（离地）**、悬挂行程恒为 `-MaxDrop`（-14）、
  轮转角恒为 0 —— 载具仿真在出输出（轮半径 20.88 读得到），悬挂扫掠却永远探不到地；
- 结果：车坐在**碰撞盒**上（盒底 z=+1cm）而不是悬挂上；`SetDriveTorque` 给油不产生位移。

**已排除**（都实测过）：
- 骨架缩放（全骨 scale=1 ✓）、刚体是否动态（已修 ✓）、整车缩放 0.5 / 1.0（都试过，症状相同）；
- `p.Vehicle.DisableConstraintSuspension 1`（关掉约束悬挂）无效；
- `p.Vehicle.DisableVehicleSleep 1`（关掉激进休眠）无效；刚体 `醒=True 模拟=True 重力=True`；
- 扫掠形状 Raycast / Spherecast 都一样；
- Python 侧对同一位置做同样半径的球扫掠**能命中地面**（simple 命中 z=2.2、complex 命中 z=-17），
  说明几何在、通道也在。

**下一步候选（按怀疑度排序）**：
1. **对比引擎自带载具模板**（本机 `Engine/Templates` 不存在，模板内容缺失）：新建一个
   空项目拉一个 Vehicle 模板，按其 PhysicsAsset / 组件配置逐项对齐 —— 最快定位"我少配了什么"。
2. **改用「资产形式」的物理资产**（编辑器里右键骨骼网格 → Create Physics Asset，人工一分钟），
   排除"运行时构建的 PA 有隐性差异"（例如 `SkeletalBodySetups` 是 `UPROPERTY(instanced)`，
   运行时 NewObject 的实例可能少了某些初始化）。
3. **核对关卡碰撞的复杂/简单数据**：插件在 `Update()` 里把 `TraceParams.bTraceComplex` **硬编码为 true**
   （`ChaosWheeledVehicleMovementComponent.cpp:1774`），而本作地面（road_hd）在 Python 探针里
   表现成「simple 命中 2.2 / complex 命中 -17」= 两层不同几何。若关卡的路面/坡道/掩体在
   **complex 通道**上不可见，12 个轮的扫掠就永远探不到地 —— 与现象完全吻合，值得优先验证。
4. 开 `p.Vehicle.ShowDebugInfo` / 看 `FVehicleDebugParams` 里其余调试开关，确认扫掠确实发出去了。

## D. 复现命令

```bash
# 骨骼网格（改动几何/骨骼后）
"/c/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python Scripts/build_tank_skeleton.py
deno run -A Scripts/editor.deno.ts execfile Scripts/tank/reset_tank_skeletal_assets.py
deno run -A Scripts/editor.deno.ts execfile Scripts/tank/import_tank_skeletal.py

# 单人 PIE（需编辑器关闭时切模式）
uv run --no-project python Scripts/tools/pie_mode.py standalone   # 载具测试
uv run --no-project python Scripts/tools/pie_mode.py listen3      # 恢复三开联机

# 自检
deno run -A Scripts/editor.deno.ts execfile Scripts/tank/pie_vehicle_report.py
```

---

## E. 2026-09-12 下午复查补充（关键进展 + 剩余卡点）

### 已修好（都经数值/截图验证）

| 问题 | 根因 | 修法 | 证据 |
|---|---|---|---|
| 整车 **roll=90°**（视觉躺倒、给油不走） | FBX 轴转换把 −90°X 烘到骨架根节点，物理刚体绑在 `root` 骨上 → 刚体空间整体转 90° | Blender 导出改 `axis_forward="X", axis_up="Z"`（声明同轴，零旋转）+ 根骨 tail 改 +Y 抵消导出器的骨骼 −90° 偏航 | 车 actor `roll=90.3°` → 0；刚体世界包围盒 Z 从 87.5 → **59**（盒正立） |
| 12 个轮 `InContact` 恒 0、悬挂恒 −MaxDrop | ① 关卡地面（road_hd / CityGate 系列）**没有简化碰撞**（当年"无碰撞靠齐平"，旧 Pawn 不吃重力没暴露）② 插件在 ComplexSweep 下把 `bTraceComplex` 硬编码为 true，而地面只有简单碰撞 | ① 给这些网格补盒简化碰撞（`StaticMeshEditorSubsystem.add_simple_collisions`；注意它**返回值是 0 但实际加了**）② 轮子改回 `SimpleSweep`（引擎默认） | 离地轮 **12/12 → 0/12**、悬挂 [−8.9..−7.0]（一致压缩） |
| 车体渲染成默认灰、履带无颜色 | 材质槽丢失：`SkeletalMaterial` 是 USTRUCT，Python 改的是副本写不进资产；`EditorAssetLibrary.save_loaded_asset` 在本工程失效 | 改为**代码里指定**槽 0=mat_61 / 槽 1=M_TrackScroll（构造函数 `SetMaterial`） | 截图里车体已恢复橄榄绿（履带待再确认） |
| 车体与负重轮上下错位（"组装不对"） | 视觉轮把悬挂行程**照符号加到 Z 上**，而车体本身已随压缩下沉 → 双重计算 | `UpdateWheelVisuals` 里取负号（压缩 = 车体降低 = 轮子相对车体升高） | 待截图复验 |
| 出生落位/悬空休眠 | PlayerStart 的 Z 是按旧 Pawn（原点=盒心）标的；悬空+零速会被载具休眠冻住 | `PostActorCreated` 里按 12 轮射线落位；有轮悬空时每帧 `SetSleeping(false)` | 落位日志 12/12 探到地面 |

**踩坑补记**：删掉骨骼网格资产后**必须重启编辑器** —— CDO 缓存的是旧资产指针，不重启会一直 `GetSkeletalMeshAsset()==NULL`（物理资产都建不起来）。

### 剩余卡点：给油不走（唯一）

现状（全部数值取证）：
- 12 轮全接地、悬挂压缩一致（−8.9..−7.0 cm）、车体静止 z 稳定（不沉不浮）；
- 日志确认每帧 `施加驱动：左=1.00 右=1.00（600 Nm/轮 ×12）`；
- 但速度恒 0、**轮子转角不增长**；
- 把 `MaxDriveTorque` 拉到 300000 Nm/轮仍 0 位移；对刚体打冲量/设速度（−800cm/s）位置也**逐帧纹丝不动** → **位姿被载具仿真每帧写回**（仿真持有位姿，且它的积分没被推动）。

**下一步（按验证成本排序，先数值后结构）**：
1. **在 C++ 自检里打印 `FWheelStatus`**（`DriveTorque` / `BrakeTorque` / `SpringForce` / `NormalizedSuspensionLength`）—— Python 侧 `to_tuple()` 是**空元组**（Chaos 的 WheelStatus 字段未暴露），这几个字段只能在 C++ 读。对着它就能确认「力矩是否真进了仿真」「弹簧力是否撑得起整车」。
2. **核对质量**：实测静态压缩 ≈8cm，若 `SpringRate=250`，反推单轮簧上质量 ≈2000kg（12 轮 ≈24t）—— 与 `Mass=4000` 不符，怀疑 `UpdateMassProperties` 没生效（物理资产盒体积 62.8m³ × 默认密度）。在 C++ 里打印 `GetBodyInstance()->GetBodyMass()` 判定。
3. 若质量正常而力矩没进仿真 → 查 `FChaosWheelSetup`/`ExternalTorqueCombineMethod=Override` 是否真的传到 `FSimpleWheelConfig`（`FillWheelSetup` 路径），或考虑改**走引擎输入**（`SetThrottleInput` + 打开 `bMechanicalSimEnabled` + 扭矩曲线）而不是自己按轮给力矩。

### 方法论教训（用户提出，确实该早做）

**关于物理/数学的判断，先在 Python（或能读到真值的地方）做数值验证，不要靠读源码推理。** 本次多次因为"从代码推断"而走偏（发光照/轴/休眠/量级四轮）；而两处最有效的定位都是**直接测量**：`roll=90.3°`、`离地轮 12/12`。注意本机 Python 读不到 `FWheelStatus`/`HitResult` 的字段（前者 `to_tuple()` 为空）—— 这类验证要落到 C++ 日志或 `to_tuple()` 可用的结构上。

---

## F. 交接状态（2026-09-12 收工，等用户判断）

### 工作区状态：**未提交**（改动都在，方便你逐项审阅/回退）

代码（`git diff --stat`）：
- 新增：`Source/Tank/TankVehicle.{h,cpp}`、`TankVehicleWheel.{h,cpp}`（载具 Pawn + 物理轮参数）
- 改：`BattleGameMode.{h,cpp}`（`bUseChaosVehicle` 开关 + `GetDefaultPawnClassForController` 钩子）、
  `TankPawn.cpp`（**「贴障碍物按 W 再点 A/D 闪到障碍物上」的修复**，见下）、`Tank.Build.cs`（+ChaosVehicles）、
  `Tank.uproject`（+ChaosVehiclesPlugin）、`Config/DefaultGame.ini`
- 脚本：`Scripts/build_tank_skeleton.py`（Blender 骨骼化）、`Scripts/tank/*`（导入/探针/实验一批）、
  `Scripts/tools/pie_mode.py`（单人/三开 PIE 切换）、`Scripts/tools/win_click.py`

内容资产（二进制，注意审阅）：
- `Content/tank/ztz-88a/ztz88a_skeletal{,_Skeleton}.uasset`（新增骨骼网格+骨架）
- **9 个导入网格被补了盒简化碰撞**：`Road/road_hd`、`CityGate/{Floor,Footing,Railing,Wall}`、
  `CityGate/door/door/StaticMeshes/{Bolts,Bolts 系列 4 个}` —— 这是「坦克陷进地面」的修复
- `Content/Lv1-TankArena.umap`：**字节数不变**（154622 → 154622），是补碰撞过程中触发的一次重存；
  期间误建的 8 个 `*_Collision` 代理已全部删除并复查干净（脚本 `check_level_clean.py`）

### 运行/开关状态

| 项 | 当前值 | 怎么切 |
|---|---|---|
| 关卡载具 | **`bUseChaosVehicle=True`**（用新载具） | `Config/DefaultGame.ini` 改 False 回旧 Pawn（改完要重启编辑器） |
| PIE 模式 | **standalone（单人）** | `uv run --no-project python Scripts/tools/pie_mode.py listen3` |
| 载具调试输入 | `DebugThrottle`/`DebugSteer`（Python 可设，替代键盘注入） | 见 `TankVehicle.h` 的 Tank\|Debug 段 |

### 待你判断的三件事

1. **驱动路线**（唯一硬卡点，详见 §E）：
   - 路线 A：继续查「力矩没进仿真 / 质量不对」（C++ 仪表已埋好：`FWheelStatus.DriveTorque`/`SpringForce` + `GetBodyMass()`）；
   - 路线 B：改走引擎标准输入（`SetThrottleInput` + 打开 `bMechanicalSimEnabled` + 扭矩曲线 + `bAffectedByEngine`），
     放弃「按轮直给力矩」；这也是官方模板的用法，代价是要做一条扭矩曲线资产。
   - 我的建议：先跑一次 A 的对账（半小时内能出结论），再决定要不要切 B。
2. **关卡碰撞的补法**：现在是「给导入网格加盒简化碰撞」（粗糙但立竿见影，坦克能站住了）。
   另一条路是在资产源头（FBX/Interior 流程）做正式碰撞，工作量大、但更干净 —— 需要你拍板。
3. **旧 Pawn 的闪移修复**（本次顺带做的）：`DepenetrateIfStuck` 从「一步抬到不阻塞」改成
   「先水平 8/16/24cm 找空位 → 水平全堵才每帧抬一步 24cm」，并加了「内缩 6cm 才算真嵌进去」的判据。
   **尚未实机验证**：三开 PIE + `bUseChaosVehicle=False`，贴着掩体按 W 再点 A/D，应看到「被墙挡一下」而不是瞬移到障碍物顶。

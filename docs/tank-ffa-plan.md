# 坦克 FFA 联机对战计划书（2~8 人个人战）

> **当前状态请看 [`STATUS.md`](STATUS.md)**（里程碑进度 / 已验证项 / 已知问题 / 下一步）。
> 本文件是设计决策与逐轮排查的完整记录，按时间顺序累积，越往后越新。
>
> 2026-09-10 定稿（同日扩容：1v1 → 2~8 人 FFA 个人战）。丧尸线冻结于 `zombie-horde` 分支（fc234f0），本文件是坦克 PvP 方向的完整计划。
> 定位：简历项目。国内 UE 岗位以网游为主，网络同步是 gameplay 程序岗刚需而非加分项。
> 后续阶梯：本项目学到的复制/RPC/权威模型是 GAS 技能游戏的全部前置知识，一点不浪费。

## 一、为什么坦克对战好做（可行性结论）

| 维度 | 坦克炮弹对战 | 对比：近战动作对战（永劫式） |
|---|---|---|
| 实体规模 | 2~8 辆坦克，复制压力趋近于零 | 同左，但状态机复杂度高 |
| 命中判定 | 炮弹延迟宽容：服务端跑弹道物理，命中结果回传即可 | 近战需要延迟补偿/回滚，netcode 最难题目 |
| 时序窗口 | 无 | 振刀/连招取消靠帧级判定 |
| 移动同步 | Pawn 的 ReplicatedMovement 引擎现成 | CharacterMovement 自带预测但要理解 |

**结论：是联机入门的最优项目。** 真正要自己搭的只有两块：自定义移动的输入上报、开火 Server RPC。

**人数扩展零增量**：服务端权威架构对人数不敏感——Server RPC、属性复制、伤害判定全是逐 Actor 逻辑，2 人和 8 人的网络代码完全一样。3+ 人多出来的工作量只有：计分板 HUD（PlayerState 驱动，UE 标准做法）、N 个出生点布局、PIE 三开测试。**不要为人数做任何特殊架构**。

## 二、范围（明确做 / 明确不做）

**做：**
- Listen Server 拓扑（主机=服务端+玩家1，其余玩家连入，目标 2~8 人，开发期 3 开验证）
- 服务端权威开火与伤害（Server RPC + 服务端炮弹 + 复制血量）
- 移动同步：M1 客户端权威快速闭环；服务端权威+预测列为 stretch
- 击杀 / 死亡 / 重生闭环，计分挂 PlayerState（天然支持 N 人）
- 胜利条件：先到 K 杀（当前配置 5 杀，见 Config/DefaultGame.ini 的 KillsToWin）；限时排名制列为可选
- 各自本机 HUD（血条、装填）+ 全员计分板
- 验收：多机 PIE 全流程 5 分钟可演示，可打包局域网试玩（可选）

**不做（写下来防蔓延）：**
- 大厅/匹配系统、账号体系
- 组队/团队模式（纯 FFA 个人战）
- 专用服务器部署与运维（Dedicated Server 打包只做可行性验证，不做运维）
- 反作弊（客户端权威阶段明确无反作弊，面试讲 trade-off 即可）
- 断线重连、跨网 NAT 穿透
- 网络预测回滚（stretch 才碰）

## 三、复用与清理清单

**原样复用（项目主角不动）：**
- `TankPawn` 全套（负重轮差速、炮塔伺服、弹道瞄准、弹簧臂相机）——已确认零丧尸依赖
- 坦克资产、材质、输入资产（/Game/tank/inputs/）、地图地形与城门装饰
- 工具链：build.deno.ts、editor.deno.ts、PIE 远程启停

**改造：**
| 现有 | 去向 | 改动 |
|---|---|---|
| `WallHealth` | `TankHealth`（挂 TankPawn） | 模式照抄，加 OnHealthChanged 复制回调 |
| `DefGameMode` | `BattleGameMode` | 换胜负规则：PlayerState.Kills 计分 + 先到 K 杀；按 PlayerController 分配环形出生点 |
| `DefHUD` | `BattleHUD` | Canvas 直绘模式保留，数据源换成：本机坦克 HP + PlayerState 计分板 |
| `TankProjectile` | 保留 | 删 `#include "Zombie.h"` 与溅射遍历（TankProjectile.cpp:3,87-99）；命中改对坦克 ApplyPointDamage |

**移除（M0 执行，涉及删除逐一确认）：**
- `Zombie/ZombieSpawner/ClimbManager/DefWall`（分支已存档）
- 地图中的 SmokeWall_Proxy 代理墙、刷怪器、残留丧尸
- smoke_*.py / probe_*.py / fix_proxy_*.py（丧尸专用调试脚本，分支可查）
- 碰撞通道约定重审：ECC_GameTraceChannel1 原是丧尸通道，坦克间改为互相 Block（PvP 要挡住对方）+ 炮弹对坦克 Block

## 四、网络架构决策

### 拓扑：Listen Server
- 开发期：PIE Net Mode = Play As Client 多开（Project Settings → PIE → Run Under One Process，3 个客户端起步验证），或独立多进程连 localhost
- 打包期（可选）：打包 Windows 包，主机开 LAN 房，其他机器直连 IP
- 主机负载：listen server 下主机既当服务端又当玩家，N 人时略高，但坦克项目无压力（8 台车 + 几发炮弹）

### 移动同步：先客户端权威，服务端权威列为 stretch
- **M1 客户端权威**：客户端本地模拟（现有 Tick 移动不动），通过 `Server RPC` 上报 Transform，服务端校验后转发给其他端（SetReplicateMovement 或手动 ReplicatedTransform）
- 理由：TankPawn 是自定义 Tick 移动（非 CharacterMovementComponent），没有免费的移动预测；客户端权威一天能闭环，演示完全够用
- 面试价值：能讲清"客户端权威 vs 服务端权威"的作弊面/手感 trade-off，比直接抄预测代码更加分
- **stretch（M5+）**：服务端权威移动 + 客户端预测 + reconciliation——做完它就是面试场上的降维打击，但不阻塞主线

### 开火与伤害：服务端权威
- 客户端开火输入 → `Server Fire(cosmetic params)` RPC
- 服务端生成炮弹（服务端模拟弹道 180m/s + 重力），Multicast 播放开炮特效/后坐力
- 炮弹命中在服务端判定：`ApplyPointDamage` 打坦克 → `TankHealth`（Replicated）扣血
- 血量变化用 `OnRep_Health` + 多播委托驱动 HUD；死亡判定在服务端，重生走 GameMode RestartPlayer

### 出生、计分与回合
- 出生点：地图外围环形均布 N 个（先按 8 个布置，人数少取前 N 个）；重生时优先选远离击杀者的点位
- 计分：**自定义 `ATankPlayerState`**，每人一份挂在自己的 PlayerState 上，天然多人扩展，
  GameMode/HUD 遍历 `PlayerArray` 即可
  > ⚠️ **勘误（2026-09-12）**：本文原写「`APlayerState::Kills`（引擎自带复制）」——
  > **UE5 里没有这个字段**。核对 5.8 源码：`APlayerState` 只剩 `Score`(float)，
  > `Kills`/`Deaths` 在引擎框架类（PlayerState / GameStateBase）里全都搜不到（UE4 之前的记忆）。
  > 因此击杀数必须自己加复制字段，见 `TankPlayerState`。
- 击杀 → 死亡表现 → 3s 重生；**重生短暂无敌 2s**（FFA 混战防落地秒杀，灰盒用闪烁材质表达）
- 胜利：先到 K 杀（`Config/DefaultGame.ini` 的 `KillsToWin`，当前 **5**）；GameMode 结算 → 全员胜利面板 → 倒计时重开

## 五、里程碑（每个里程碑结束 = 一个可玩状态）

### M0 清理与地基（约半天）
- 移除丧尸系统与地图残留（清理清单见上）
- `TankHealth` 组件 + 挂到 TankPawn；坦克间碰撞改 Block
- `BattleGameMode` 骨架：多 Pawn 出生（PIE 三开能各开一辆）
- **DoD**：PIE 三开，三辆坦克各自受控、互撞互挡

### M1 移动同步（1~2 天）
- Server RPC 上报 Transform + ReplicatedMovement
- Owner/非 Owner 视角确认：本机即时、远端平滑（NetworkSmoothingMode 调参）
- **DoD**：PIE 三开各自开坦克，所有远端坦克位置/朝向/姿态同步无瞬移

### M2 战斗闭环（2~3 天）
- Server Fire RPC + 服务端炮弹 + Multicast 特效（炮口焰/后坐力本机表现已有）
- 服务端命中扣血、OnRep 驱动 HUD、死亡击飞表现、重生
- **DoD**：三车混战互射，扣血→死亡→重生全链路，血量全员一致

### M3 计分与 HUD（1~2 天）
- PlayerState.Kills 计分、先到 K 杀胜利（当前 5）、回合重置、重生保护
- HUD：本机血条+装填、计分板（Tab 呼出或常驻角落）、死亡/重生提示、胜利面板
- **DoD**：三车完整一局 3~5 分钟，计分板实时正确，胜负面板与重开正确

> **进度（2026-09-12）——M3 全部完成，逻辑与 HUD 均已验证**
>
> 逻辑层：`ATankPlayerState`（Kills/Deaths 复制）、`ATankGameState`（回合状态复制）、
> `ABattleGameMode::NotifyKill` / `DeclareWinner` / `ResetRound`、重生保护。
> 规则参数 `KillsToWin` / `RoundRestartDelay` 走 `Config`，在 `Config/DefaultGame.ini`
> 的 `[/Script/Tank.BattleGameMode]` 段里改，**不用重编**。
>
> HUD（`BattleHUD`，Canvas 直绘零资产）：
> 左下角血条 + `HP x/y` + 装填进度 + 重生保护提示；右上角常驻计分板
> （`GameState->PlayerArray` 驱动，击杀降序、本机高亮）；屏幕中央阵亡/重生提示；
> 回合结束的胜利面板（读 `TankGameState`）；其他玩家头顶血条。
>
> 验证（PIE 三玩家实跑）：计分归属正确 → 达标触发胜利且 GameState 复制到客户端 →
> 5.0s 后回合重置（战绩归零 + 全体回炉重生）→ 每次出生带 2s 保护。
> HUD 用编辑器内截图逐项肉眼确认（计分板数值与高亮、血条/装填条、胜利面板、阵亡提示、
> 回合重置后全员归零），截图在 `Saved/Screenshots/WindowsEditor/`。
>
> **实现期踩到的坑**：
> 1. `AGameMode` 已有 `virtual void EndMatch()`（引擎 MatchState 状态机）。胜利函数若同名会
>    **隐藏**基类虚函数（C4263/C4264），已改名 `DeclareWinner`。灰盒刻意不走引擎
>    MatchState/`RestartGame`——`RestartGame` 会触发地图重载，对「原地重开一局」太重；
>    M4 若要完整回合流程（含回大厅）可切回。
> 2. Python 侧属性名：`bMatchOver` → `match_over`、`bSpawnProtected` → `spawn_protected`
>    （UHT 剥掉 `b` 前缀），写 `b_match_over` 会报 "Failed to find property"。
>
> **两个遗留小问题已修**（2026-09-12 第三增量）：
> 1. **阵亡窗口相机朝天** —— 根因：`APlayerController::CalcCamera`（PlayerController.cpp:994）
>    在无 Pawn 时退化成 `(GetFocalLocation(), GetControlRotation())`，而 PC 从未被移动过。
>    注意 `AController::SetActorLocation` 是 **private**，挪不动 PC；解法是新增
>    `TankPlayerController` 覆盖这两个函数，阵亡期间返回"最后的观战位姿"
>    （取残骸后上方：退后 900 / 抬高 350，否则镜头会卡在爆炸球内部），`OnPossess` 时清掉。
> 2. **爆炸球不消失** —— `DrawDebugSphere(..., bPersistentLines = true, LifeTime = 2.0f, ...)`：
>    `true` 的语义是"持久线直到 Flush"，**LifeTime 不生效**。改成 `false` 即按 2s 消失。
>
> **验证工具坑（重要）**：从 Python 文件桥调用 `unreal.AutomationLibrary.take_high_res_screenshot`
> 会让编辑器在约 50~65s 后 `EXCEPTION_ACCESS_VIOLATION` 崩溃（callstack 最内层 python311.dll、
> 无任何游戏代码帧、故障地址是 ASCII 文本 = 悬空引用）。无截图的长会话不崩，与游戏代码无关；
> 把 latent 代理存进模块级列表只能降低概率、不能根除。**截图只当开发期辅助**，截前先存东西，
> 崩了重启即可。截图脚本见 `Scripts/pie/pie_*.py` 与 `Content/Python/tank_shot.py`。

### M4 打磨与包装（弹性）
- 地图 FFA 化：环形出生点、中央/四角掩体布置、手感参数平衡
- （可选）Windows 打包 + 局域网真机试玩（3 台机器最有说服力）
- （stretch）服务端权威移动 + 预测回滚
- **DoD**：给面试官的 5 分钟演示脚本（开三个客户端：移动同步→混战互射→抢击杀→计分板变化→胜利）

> **进度（2026-09-12）——地图 FFA 化已完成**
>
> 改前实测：场地只有 **3 个出生点且不均布**（r=2110/2110/1100），**完全没有掩体**
> （33 个静态网格全是城门/公路/天空球/地面）。
>
> 改后：
> - **出生环**：圆心 (0,4600)、半径 2800、8 点按 22.5° 起偏均布、全部朝圆心。
>   复核：8 点半径极差 **0.0**、相邻夹角全 **45.0°**（数学上完全均布）。
> - **掩体**：中央矮墙 2 块（各 1400×400×500，X=±1600）+ 四角方块（800×800×500）。
>   出生点到最近掩体 **1119cm**（> 车长 760）。
> - **通道**：中央大道 1800cm + 两侧迂回各 800cm，全部可通行。
>
> **自己抓到的设计缺陷**：初版中央矮墙是 3 块、只留 200cm 缝——而坦克宽 **350cm**，
> 那两道缝根本过不去（看着像通路实际堵死）。改成 2 块后写了间隙复核脚本按车宽自动判定。
> **教训：掩体间隙按车宽算，别只看图。**
>
> 实跑验证：3 辆车分别落在环上的 (2587,5672) / (-2587,3528) / (-1072,7187)
> （22.5°/202.5°/112.5°），顺带又验证了一次最远点选点生效。
> 截图见 `Saved/Screenshots/WindowsEditor/level_final.png`。
>
> 回滚：关卡改动只有 `Content/Lv1-TankArena.umap` 一个文件（Content 在 git 内）。
>
> 待办：手感参数平衡（需实玩）、（可选）打包、演示脚本。
> 「远端负重轮不转」已随 Tick 自愈一并解决（远端车现在有 Tick，第 8 段的位移反推生效）。

## 六、风险与坑（预判）

| 风险 | 对策 |
|---|---|
| Live Coding DLL 锁 | 构建前关编辑器（已有标准流程） |
| PIE 多开窗口/日志混乱 | 3 开起固定窗口位置；日志按 LogNet 过滤；探针脚本区分 world（PIE 期 get_editor_world()=None 的坑已知） |
| 客户端权威下远端抖动 | NetworkSmoothingMode + ReplicatedMovement 的插值；必要时手动缓冲插值 |
| 炮弹双端生成导致双重伤害 | **只在服务端 Spawn 炮弹**，客户端零炮弹逻辑；特效走 Multicast |
| 输入在非 Owner 端误触发 | SetupPlayerInputComponent 只在 Possessed 端生效（引擎保证），但 Tick 里坦克轮子等视觉更新要确认非 Owner 端也跑 |
| FFA 误伤与抢人头判定 | 击杀归属=伤害最后一击的 Instigator；友军伤害不存在（无友军），但重生保护要防落地秒杀 |
| 一轮只动一个变量铁律 | 每个里程碑内一次只改一个系统；M1 没验证完不碰 M2 |

## 七、面试话题映射（每个实现点对应高频面试题）

| 实现点 | 面试可答 |
|---|---|
| Role/Authority 判定 | 客户端/服务端/独立端各自跑什么逻辑 |
| GetLifetimeReplicatedProps | 属性复制条件（COND_OwnerOnly 等） |
| Server RPC vs Multicast | 为什么开火走 Server、特效走 Multicast、可靠性级别怎么选 |
| ReplicatedMovement | Pawn 移动同步的引擎机制与插值 |
| PlayerState 计分板 | PlayerState/PlayerController/GameState 三者职责与 PlayerArray 同步时机 |
| 客户端权威 trade-off | 为什么先客户端权威：作弊面 vs 开发成本 vs 手感 |
| （stretch）预测回滚 | 误判补偿、reconciliation——答上来就是加分项 |
| GAS 衔接 | 下一项目的 GameplayEffect 属性复制直接复用本项目心智模型 |

## 八、与丧尸线的关系

- `zombie-horde` 分支永久冻结：P1 完整闭环（刷怪→攻墙→胜负）+ P2a 槽位堆系统（334 槽位网格+攀爬 FSM）
- 面试叙事："做过单机 AI 波次攻防（行为链/状态机/群体槽位算法，见博客），正在做联机多人 PvP（复制/RPC/权威模型）"——一单机一联机，故事完整
- 未来若做"联机坦克守城 PvE"，丧尸线在分支上可直接复活接入

---

# 附：M2 未结案问题记录（2026-09-10，wip 1ebd562）

## 症状

死亡重生流程触发**无关玩家的占有异常**：
1. 打爆 Client1/2 → 主机坦克被"重新传送"到重生点（用户观感为瞬移）
2. 修复选点后新症状：**主机位置出现两辆坦克堆叠**
3. 日志可见神秘 UnPossession：无关坦克在他人死亡前后 1s 内被解除占有（TankPawn_7/8/10/11 均中招）

## 已确认的事实（日志实锤）

- 死亡→销毁→补发链路本身工作：血量耗尽 → 击毁（击杀者归属正确）→ 0.2s 销毁 → 巡检补发 → 新坦克满血
- 血量复制全端一致（OnRep 日志双份=多端收敛）✓
- 神秘 UnPossession 会在**非死亡玩家**的坦克上随机出现（t=383.2/177.1 等无规律时刻）
- 巡检补发的新坦克出现过"挂载→同帧 UnPossessed→被孤儿清理销毁→2s 后再补发"的循环（TankPawn_9/10/11/12）

## 已尝试（均未根治）

| 尝试 | 结果 |
|---|---|
| 三段式巡检（收编脱钩+补发+清孤儿） | 加剧——收编/清理互相打架，玩家陷入重生循环（ecca09e 后回退） |
| 选点改"离所有存活坦克最远"（替代轮转） | 传送减少但未根治；且出现堆叠新症状 |
| 巡检简化为单段（仅 GetPawn 为空才 RestartPlayer） | 仍复现 |

## 关键线索（下轮从这里入手）

**堆叠症状 = 冒烟枪**：最远点选点会收集"有 Controller 的坦克"位置，新坦克却叠在主机坦克上
→ 说明选点时**主机坦克的 GetController() 为 null** → 主机坦克处于神秘 UnPossession 态。
即：堆叠不是独立 bug，是神秘 UnPossession 的下游症状。

## 下轮排查计划（按优先级）

1. **换重生模型（最优先）**：死亡不销毁 Pawn，改为"重置"——瞬移到最远出生点 + CurrentHealth 恢复 + PendingPushOffset 清零。
   彻底绕开 possess/destroy 机器，销毁路径只剩蓝图级测试手段。若重置模型下问题消失 → 实锤销毁/占有链路是病根
2. **隔离变量**：三开不射击不碰撞，挂机 60s 观察是否仍有神秘 UnPossession（区分"战斗触发"还是"PIE 引擎定时发作"）
3. **UnPossessed 日志增强**：打印 GetWorld 帧号 + Owner 链 + 是否 PendingKill，与巡检/死亡日志对帧
4. **嫌疑复查**：ServerPushTank 的跨 Actor 指针 RPC 参数是否可能在复制通道上产生副作用（低概率但未排除）
5. M0 曾确认"PIE 多开下主机 PC 被引擎重建一次"——复查该重建是否为**周期性**（若是，引擎层无解，重置模型+巡检兜底即最终方案）

## 灰盒可接受的退路

若引擎侧不可控：死亡重置模型（方案 1）+ 单段巡检兜底， 即便底层占有偶发抽风，
玩家侧表现收敛为"偶尔重生"，可接受进入 M3（计分板），M4 再回头根治。

---

# 结案（2026-09-11）：根因 = AutoPossessPlayer，不需要重置模型

**结论：上面"下轮排查计划"的方案 1（死亡重置模型）不需要做了。** 根因在 TankPawn 构造里的
`AutoPossessPlayer = EAutoReceiveInput::Player0`，与销毁/占有链路无关，改一行即闭环。

## 证据链（对照 UE 5.8 引擎源码）

1. `APawn::PreInitializeComponents()`（Pawn.cpp:116）
   ```cpp
   if (AutoPossessPlayer != EAutoReceiveInput::Disabled && GetNetMode() != NM_Client)
   {
       const int32 PlayerIndex = int32(AutoPossessPlayer.GetValue()) - 1;   // Player0 → 0
       APlayerController* PC = UGameplayStatics::GetPlayerController(this, PlayerIndex);
       if (PC) { PC->Possess(this); }
   }
   ```
   **每个**服务端 Spawn 的坦克都会执行这段，把 Player0（主机）的 PC 抢过来。

2. `AController::OnPossess()`（Controller.cpp:356）
   ```cpp
   if (bNewPawn && GetPawn() != nullptr) { UnPossess(); }   // 换成别的 Pawn 前先解占有
   ```

3. 于是「为客户端重生而 Spawn 坦克」= 顺带把主机正在开的坦克就地 UnPossess。
   这解释了日志里 t=383.2/177.1 那种与死亡无关的神秘 UnPossession。

## 三个症状的统一解释

| 症状 | 机制 |
|---|---|
| 主机坦克神秘 UnPossession | 步骤 1+2：新坦克抢走主机 PC，主机旧坦克被 UnPossess |
| 主机位置两辆坦克堆叠 | 孤儿坦克 `GetController()==null` → `ChoosePlayerStart` 的存活坦克统计漏掉它 → 新坦克按"离存活坦克最远"选点，正好落在孤儿身上 |
| 巡检补发出现"挂载→同帧 UnPossessed"循环 | 补发时又被 Player0 抢占，PC 归属在 2s 巡检间反复横跳（三段式巡检加剧了这点） |

原文的"冒烟枪"推断（"选点时主机坦克 GetController() 为 null"）方向完全正确，
只是当时没往上追一层到 AutoPossessPlayer。

## 修复

`TankPawn.cpp` 构造函数：
```cpp
AutoPossessPlayer = EAutoReceiveInput::Disabled;   // 占有权完全交给 GameMode.RestartPlayer
AutoPossessAI     = EAutoPossessAI::Disabled;      // 配套堵住 AI 自动占有
```

**为什么必须同时关 AutoPossessAI**：`AutoPossessPlayer` 一旦为 Disabled，引擎
`APawn::PostInitializeComponents` 就放开了这条分支——
`AutoPossessPlayer == Disabled && AutoPossessAI != Disabled && GetController() == nullptr`。
而 `APawn` 的 `AutoPossessAI` 默认是 `PlacedInWorld`、`AIControllerClass` 默认解析为
`/Script/AIModule.AIController`。开局阶段 `World->bStartup` 为真 → 出生被判成 "PlacedInWorld"
→ `SpawnDefaultController()` 生一个 AIController 来抢占有。只改一行会引入新问题。

**安全性**：全项目扫描确认没有任何 uasset/umap 引用 `TankPawn` 或序列化 `AutoPossessPlayer`，
即不存在 Blueprint 子类覆盖构造函数默认值的情况；`DefaultPawnClass` 由 C++ 的
`ABattleGameMode` 提供。修复无条件生效。

## 同批修掉的附带问题

| 文件 | 问题 | 处理 |
|---|---|---|
| `TankHealth.h` | `bDepleted` 不复制 → 客户端 `IsDepleted()` 恒 false，头顶血条不剔除阵亡坦克 | `IsDepleted()` 改为 `bDepleted \|\| CurrentHealth <= 0`（CurrentHealth 是复制的，两端一致） |
| `TankPawn.cpp` | `MoveForward` 逐帧 Log 级打印（单局 1114 行刷屏） | 降为 Verbose |
| `TankPawn.cpp` | `UnPossessed` 埋点信息不足 | 降为 Log 并补 Frame/PendingKill，便于回归验证 |
| `TankPawn.h/.cpp` | `CurrentPitchInput` 只被声明和清零、从未赋值 → Tick 里那段分支是死代码 | 删除该死状态与分支（`ElevateGun` 直接改 `DesiredGunPitch` 才是真实路径） |
| `TankPawn.cpp` | 阵亡到销毁的 0.2s 窗口内仍可开火 | `Fire()` 加 `IsDepleted()` 早退 |

## 遗留（未动，供后续判断）

- 远端坦克的负重轮不转：轮速由 `CurrentMoveInput` 驱动，而该值只在本地受控实例上非零。
  纯表现问题，M4 打磨期用复制位移差反推轮速即可。
- 死亡后 PC 无 Pawn 的窗口最长 2s（巡检周期），期间客户端无 ViewTarget。
  属 M3 的"死亡/重生提示"范围。

---

# 第二轮排查（2026-09-11）：另有两个真 bug

修完占有链路后顺着引擎源码继续核对，又挖出两个独立缺陷，都已修复并重新编译链接。

## Bug 2：FFA 选点在重生时根本不会执行

`AGameModeBase::Login` 里引擎已经把首次选中的出生点存进了 PlayerController：
```cpp
AActor* const StartSpot = FindPlayerStart(Player, Portal);
if (StartSpot != nullptr) { Player->StartSpot = StartSpot; }
```
而 `FindPlayerStart` 开头就是：
```cpp
if (ShouldSpawnAtStartSpot(Player))          // 默认实现：return Player->StartSpot != nullptr;
{
    if (AActor* PlayerStartSpot = Player->StartSpot.Get()) { return PlayerStartSpot; }
}
AActor* BestStart = ChoosePlayerStart(Player);   // ← 永远走不到
```
`AGameMode` 并没有覆盖 `ShouldSpawnAtStartSpot`（已核对 GameMode.cpp），所以从第二次重生起
`ChoosePlayerStart_Implementation` **一次都不会被调用**——"离所有存活坦克最远"的 FFA 选点
等于死代码，所有重生都固定回自己最初的出生点。

**修复**：`BattleGameMode` 覆盖 `ShouldSpawnAtStartSpot` 返回 `false`，让每次重生都重新选点。

> 这条也解释了当时"改成最远点后传送减少但未根治"的观感：改动只在**首次出生**生效过。

## Bug 3：头顶血条的投影判据写反了

`UCanvas::Project` 的实现（Canvas.cpp:1890）：
```cpp
FVector resultVec(V);                 // FPlane : public FVector，切片拷贝 → 取 (X,Y,Z)
resultVec.X = (ClipX/2) + (resultVec.X*(ClipX/2));
resultVec.Y *= -1.f * GProjectionSignY;
resultVec.Y = (ClipY/2) + (resultVec.Y*(ClipY/2));
// if behind the screen, clamp depth to the screen
if (bClampToZeroPlane && V.W <= 0.0f) { resultVec.Z = 0.0f; }
```
即 `Projected.Z` 是**裁剪空间 NDC 深度**，并且**只在点在相机身后（`V.W <= 0`）时才被夹成 0**。
原代码写的是 `Projected.Z == 0.0f`，语义正好反了：只有坦克跑到身后才通过判据，
正前方的坦克永远画不出头顶血条。

**修复**：判据改为 `Projected.Z > 0.0f`。

## 已确认**不是** bug 的两处（省得后面重复怀疑）

- **炮弹 `BlockAllDynamic` 不会穿地面**。一度怀疑它忽略 WorldStatic（UE4 时代的印象），
  核对 `BaseEngine.ini` 与 `CollisionProfile.cpp` 后确认：`BlockAll` 与 `BlockAllDynamic`
  的 `CustomResponses` 都为空、响应取同一份默认容器，**两者只差 ObjectType**
  （WorldStatic vs WorldDynamic），对 WorldStatic 的响应同样是 Block。
  项目里那句"显式对 GameTraceChannel1 Block"补的是**自定义通道**（自定义通道默认响应可能不是 Block），补得对。
- **`TankProjectile` 的伤害/销毁链路**无问题：命中判定、`ApplyPointDamage`、
  击杀者归属（Instigator 链）都符合预期。

## 本轮同时补上的配置项

`Config/DefaultEngine.ini` 增加 `GlobalDefaultGameMode=/Script/Tank.BattleGameMode`。
关卡 WorldSettings 的 `DefaultGameMode` 仍然优先，**当前地图行为不变**；
补它是为了新建地图与打包时不至于退回 `AGameModeBase` 而生不出坦克（M4 打包前置）。

---

# 运行时验证（2026-09-11）：开局占有链路已确认修复

用编辑器内文件桥（`Content/Python/init_unreal.py`）跑 Python 实测，脚本在 `Scripts/`。

## 断言 1：修复真的进了已加载模块 —— PASS

```
TankPawn.auto_possess_player = AutoReceiveInput.DISABLED
TankPawn.auto_possess_ai     = AutoPossessAI.DISABLED
```

## 断言 2：PIE 三玩家实跑，占有链路无异常 —— PASS

按已保存的 PIE 设置（`PlayNetMode=PIE_ListenServer` / `RunUnderOneProcess=True` /
`PlayNumberOfClients=3`）跑了一局约 95 秒：

| 检查项 | 结果 |
|---|---|
| 服务端世界坦克数 | 3 |
| 服务端世界 PlayerController 数 | 3 |
| 每辆坦克的 Controller | 全部有主，**孤儿 0** |
| 全场坦克堆叠对数 | **0** |
| 非死亡触发的 UnPossessed | **0** |

> **认知修正**：`PlayNumberOfClients=3` + ListenServer 是 **3 个玩家总数**
> （1 server + 2 client = 3 个 PIE 世界），不是 1+3=4。
>
> **另一个坑**：客户端世界里远端 Pawn 的 `Controller` 为 null 是**正常的**——
> 远端 PlayerController 根本不会复制到客户端。孤儿判定只能在服务端世界做，
> 否则会把正常坦克误报成孤儿（第一版巡检脚本就栽在这）。

## 退出时的 UnPossessed 是正常拆卸，不是 bug

结束 PIE 时日志会出现数条 `被 UnPossessed！PendingKill=0`，但每条紧邻
`LogWorld: BeginTearingDown for /Game/UEDPIE_N_...` + `Window 'Tank Preview [NetMode: Client N]' being destroyed`，
且同一帧、伴随 `World NetDriver shutdown` / `HostClosedConnection`。
与原 bug 的区分点：原 bug 是**随机单辆、游戏中、与别人死亡相关**；这里是 3 辆同帧一起、退出时。

## 重生路径验证（2026-09-12 补完）—— PASS

昨天只验了开局占有，今天把「为客户端重生而 Spawn 坦克」这条原 bug 的实际触发路径也跑通了。

| 场景 | 结果 |
|---|---|
| 打死客户端1的坦克 | 主机坦克 `TankPawn_0` **保持** `PlayerController_0`；新车正确归 `PlayerController_1` |
| 三辆同时打死 | 三辆新车 1:1 归属正确、落点分散（`-1800,5000` / `1800,5000` / `0,7200`） |
| 孤儿坦克（服务端世界 `Controller == null`） | 全程 **0** |
| 堆叠（两两距离 < 200cm） | 全程 **0 对** |
| 游戏中非死亡 UnPossession | **0**（15 条 UnPossessed 全部 `PendingKill=1`，即阵亡车被销毁时的正常解占有） |

### Bug 2（FFA 选点）的决定性判别实验

前两次尝试都不可用，记录以免重踩：

1. **瞬移客户端坦克聚堆 → 无效**。客户端权威：服务器端瞬移会被客户端 50Hz 的
   `ServerSyncTransform` 立刻覆盖回去。
2. **只杀主机看落点 → 无判别力**。出生点环形对称，两种行为落点恰好相同。

**有效设计**：把**主机自己的**坦克瞬移到客户端1出生点旁 80cm（主机车服务端本地控制，
瞬移不会被覆盖），再打死客户端1的车 —— 若覆盖未生效，新车会回自己的 `StartSpot (1800,5000)`
叠在主机车上；若生效则走 `ChoosePlayerStart` 最远点。
实测新车落在 **(-1800, 5000)**（距主机车 3680cm）→ **覆盖确认生效**。

### 日志解读的两个坑

- **每个 PIE 世界各自给 Actor 命名**，日志里会出现多个 `TankPawn_0`，且同一 editor Frame 下
  不同世界的 `t=`（世界秒）不同。别把它们当成同一辆车。
- 结束 PIE 必刷一批 `UnPossessed PendingKill=0`，紧邻 `BeginTearingDown` + 窗口销毁，属正常拆卸。
  **判据看 `PendingKill`**：真 bug 是**活车**（`PendingKill=0`）在游戏中随机被解占有。

## M2 结论

占有 / 重生 / 伤害 / 血量同步全链路已端到端验证通过，**M2 可判定完成**，进入 M3
（`PlayerState.Kills` 计分、先到 K 杀胜利（当前 5）、回合重置、重生保护、HUD 计分板）。



# 第三轮排查（2026-09-12）：血条穿墙 + 客户端只能开炮不能移动

## Fix A：头顶血条透过建筑可见

**根因**：`ABattleHUD::DrawOverheadBars` 是 Canvas 直绘，没有深度测试，原实现只做了投影 + 80m 距离剔除，
所以建筑后面的坦克血条照样画在屏幕上。

**修法**：对每个已通过筛选的目标，从相机 POV 向血条锚点（`TankLoc + 240`）打一条 `ECC_Visibility`
射线，命中就不画；忽略自身 Pawn 与目标 Actor 本身。

**为什么用 Visibility 通道**：`BaseEngine.ini` 里 `Pawn` profile 的
`CustomResponses=((Channel="Visibility",Response=ECR_Ignore))` —— 坦克之间不会互相遮挡血条；
而建筑/掩体（WorldStatic / BlockAll）默认 Block，正是要剔除的对象。即「只剔墙，不剔车」。

## Fix B：client1/2 只能开炮不能移动

### 证据（PIE 三开，05.14–05.29 会话日志）

| 观察 | 结果 |
|---|---|
| 客户端 `ServerFire` | **成功**，并在 05.29:22 打中主机 → 客户端 IMC / 输入绑定 / Server RPC 全链路正常 |
| 客户端坦克位置 | 36s 内原地不动（开火炮口坐标 = 出生点炮口）；主机打它的 4 发弹着点 X 恒为 -2206.863 |
| 客户端2 坦克 | 4 发弹着点 Y 恒为 7011.863 → 同样全程钉在出生点 |
| **世界级对比** | M1 会话（09-10）3 世界 × 8 辆 = **24 条** `TankPawn initialized`；今天只有服务器世界 **5 条** |

### 根因（已证实，不再是推断）

**客户端世界里的坦克全部是「网络生成」，`Tick` 函数根本没被注册**（PIE 实测 `Tick 注册 0 / 启用 0`）。
而 `Tick` 是位移 / 炮塔 / 视口 / 后坐力复位 / 负重轮的唯一入口，输入回调
（`UInputComponent` 事件）不走 Tick —— 这正好解释「能开炮、不能动」。
当时留的两个候选，本轮被埋点直接判死：**(a) 成立，(b) 证伪**。

日志原文（Role=2 = 本端客户端）：

```
[Input] TankPawn_0 客户端就绪自愈：Tick 注册 0→1 / 启用 0→1，输入组件补建，动作绑定补绑，IMC新挂
```

即同一辆车到达时**五项全缺**：Tick 未注册、未启用、`InputComponent` 没建、动作没绑、IMC 没挂。

### 修法

客户端可达钩子上的幂等自愈 `EnsureClientReady()`（两个闸门，与技能推荐写法一致：
`PostNetReceive` 每收到复制更新都调、含首次；一次性初始化用标志位守住）：

| 段 | 闸门 | 覆盖范围 | 动作 |
|---|---|---|---|
| Tick 自愈 | `bTickRepaired` | 客户端世界里的**每一辆**坦克（含远端他机） | `IsTickFunctionRegistered()` 为假才补注册、`IsActorTickEnabled()` 为假才补启用 |
| 输入自愈 | `bClientReady` | **只对「本机玩家的本机 Pawn」** | `InputComponent == nullptr` → 补建；`GetActionEventBindings().Num() == 0` → 补绑；补挂 IMC |

> **为什么 Tick 段必须覆盖远端车**（第一版漏了这点，属错误设计，已修）：
> 炮塔 / 火炮朝向是在 Tick 里 `SetRelativeRotation` 落地的。只修自己的车会出现
> 「复制值到了、`TurretPivot` 不动」—— 实测另一客户端看这辆车 `net_turret_yaw = 102.1`
> 而 `current_turret_yaw = 0`，炮塔就是不转。详见第五批 §2。

> 为什么不用 `PostNetInit`：引擎里只有 DataChannel / DemoNetDriver 会调它，常规复制路径不走；
> `BeginPlay` 在客户端又可能早于复制状态到达。`PostNetReceive` 是客户端唯一的常规复制钩子。

### 验收埋点与判读（实测结果见右列）

```
[Net]   TankPawn_x Tick 自愈：注册 0→1 / 启用 0→1（Role=2 本机 / Role=1 远端）
[Input] TankPawn_x 输入自愈：输入组件补建，动作绑定补绑，IMC新挂（Role=2）
[Input] TankPawn_x 本机位移首次生效：本帧 x.xcm（被阻挡=0）
```

| 观察到 | 结论 |
|---|---|
| `Tick 注册 0→1` | 假设 (a) 成立 —— **实测已出现，根因确认** |
| 出现「本机位移首次生效」 | Fix B 生效 —— **实测已出现**（投 W 1.5s → 前进 2670cm，服务器侧同步） |
| 全是「已在/新挂」+ 无位移日志 | 假设 (b) WASD 没投递 —— **实测未出现，已排除** |


## 顺带发现（本次未改）

- `LogGameMode: Error: Mixing AGameStateBase with AGameMode`：`ATankGameState` 继承 `AGameStateBase`，
  而 GameMode 是 `AGameMode`。引擎建议改继承 `AGameState`（或两边统一用 Base）。与本次两个 bug 无关。
- ~~本机 UBT 在此工作区跑不通~~ —— **该结论已作废**，见文末「第五批 / 构建」。真实原因是
  `Scripts/build.deno.ts` 的 `sh()` 只收 stdout、把 MSVC 走 stderr 的诊断丢了，加上前一次链接失败
  留下的 0 字节 `.lib`/`.pdb` 会毒化后续每一次构建。

# 第四批（2026-09-12 14:2x）：胜利条件 / 炮塔朝向同步 / 三面边界台阶

## 1. 先到 5 杀

`Config/DefaultGame.ini`：`KillsToWin=10 → 5`。该属性是 `UPROPERTY(Config)`，只在引擎启动时读一次，
**改完要重启编辑器**（HUD 的「先到 N 杀」读的是 GameState 里由 GameMode 同步过去的值，自动跟随）。

## 2. 敌方视角看不到炮塔转向 —— 已修

**根因（两层）**：

1. 炮塔 yaw / 火炮 pitch 只由本机输入驱动，服务器**完全不知道**（只有开火时随 `ServerFire` 报一次炮口位姿）；
2. 更要命的是远端的 Tick 也在跑那套「电驱伺服」，而远端的 `CameraRelativeYaw` / `DesiredGunPitch`
   恒为 0 —— 伺服把炮塔一路拽回车头、火炮压回 0°，所以别人看到的永远是「炮塔朝车头」。

**修法**（复用 M1 位姿上报链路，不新开 RPC）：

| 环节 | 做法 |
|---|---|
| 上报 | `ServerSyncTransform` 增加 `TurretYaw` / `GunPitch` 两个参数（Unreliable 50Hz，与位姿同频） |
| 存储/转发 | 服务器写入 `NetTurretYaw` / `NetGunPitch`，`DOREPLIFETIME_CONDITION(..., COND_SkipOwner)` |
| 应用 | Tick 里 `IsLocallyControlled()` 才跑伺服；其余实例直接采用复制值 |
| 校验 | `_Validate` 增加两个 float 的 NaN 检查 |

`COND_SkipOwner` 的理由：拥有者本地自己算，不需要吃一次回传，省掉一半带宽。
复制值本身就是对端伺服后的结果，远端不再插值（再插值只会加滞后）。

## 3. 场地三面加「小台阶」防越界 —— 已执行（`Scripts/level/level_edge_step.py`）

实测：`CityGate_Wall` 占 Y -409~1339、X -3613~3593 —— 南侧封死，**而且城墙比场地还宽**，
所以东西两道台阶只要压进城墙北表面就不会在角落留缝（脚本直接读城墙包围盒来定南端起点）。

| 台阶 | 中心 | 尺寸 (cm) |
|---|---|---|
| `Bound_West` | (-3069, 4426) | 200 x 6356 x 100 |
| `Bound_East` | ( 3069, 4426) | 200 x 6356 x 100 |
| `Bound_North` | (0, 7504) | 6338 x 200 x 100 |

高 100、厚 200，外表面比地边内缩 20cm（保证整块站在地面上）→ **可行驶边界 X ∈ [-2969, 2969]、Y ≤ 7404**。
脚本可重复执行（先按 `Bound_` 前缀清旧件）。

**顺带修正出生环半径 2800 → 2300**：半径 2800 时北侧两个出生点 (Y=7186) 的车尾投影已到 Y≈7604，
离地面北边 (7624) 只剩 20cm —— 既放不下台阶，车本身也几乎悬空。缩到 2300 后，
出生点到台阶内表面最近仍有 261cm（北）/ 426cm（东西）。

## 本轮构建踩坑

- 改 `UFUNCTION` 签名 / 新增 `UPROPERTY` 后，**必须先跑一次 UBT 让 UHT 重生成代码**，
  否则直调 cl 会报 `C2511: 没有找到重载的成员函数`（生成头停在旧签名）。
  UHT 的报错是真错误：本轮它抓出 `BlueprintReadOnly should not be used on private members`
  → private 段里的 UPROPERTY 需加 `meta = (AllowPrivateAccess = "true")`。
- 编辑器开着时 `UnrealEditor-Tank.dll` 被锁，link 报 `LNK1104`；
  `UPROPERTY(Config)` 也只在启动时读 → **代码与 ini 改动都要等编辑器关闭后重链、重启才生效**。

---

# 第五批（2026-09-12 14:25 起）：接管编辑器/构建脚本，并修掉炮塔复制的「最后一公里」

## 0. 项目本来就有接管脚本，两个都有真 bug

`Scripts/editor.deno.ts`（`open/close/status/exec/execfile`）与 `Scripts/build.deno.ts` 一直都在。
本轮把它们修到能用，编辑器/构建/验证现在可以全程自跑。

| 问题 | 症状 | 修法 |
|---|---|---|
| `editor.deno.ts open` 拉起的编辑器会自己死 | `MCP 8000 就绪` 后约 40s 进程消失，`Tank.log` 停在模块加载中途、**无任何 shutdown 记录** | `Deno.Command spawn` 的子进程挂在调用方 Job Object 上，改用 `Invoke-CimMethod Win32_Process Create`（父进程 WmiPrvSE，job 之外） |
| 构建失败**没有任何诊断** | 只剩一句 `Exited with error code 1` | 脚本的 `sh()` 只读 stdout；MSVC 诊断走 stderr → 改为 stdout+stderr 合收、优先打印 error 行 |
| 兜底构建「不脏就不编」 | 改了 `.cpp` 也被跳过 | 原先只比 rsp 的 mtime（rsp 不随源码更新）；改为比源文件 + `Source/Tank` 最新时间 |
| 兜底构建被 `C1083` 中断 | rsp 指向的源文件早已删除（`ClimbManager/DefGameMode/DefHUD/DefWall/Zombie/ZombieSpawner`，本项目更早形态的残留） | 识别并跳过陈旧 rsp；那 6 组 `.obj.rsp`/`.obj` 已手工删除 |
| 兜底链接报 `LNK1136: invalid or corrupt file` | 上一次 UBT 链接失败**把 `.lib`/`.pdb` 截断成 0 字节**，并已删掉旧 DLL | 构建开始前统一清 0 字节产物 |
| 链接顺序 | —— | 改回 `link /LIB` → `link dll`（DLL 链接按 `/IMPLIB` 覆写导入库，反序会让导入库落后） |

**修正上一批的结论**：「本机 UBT 跑不通」不准确。清掉坏产物后 UBT 能成功（实测 `Result: Succeeded`
并真的重链）；只有**需要真正 link** 时才会因 `LNK1201`（写 PDB）失败，而兜底路径现已能可靠接管
（强制重建实测：10 个 TU 重编 + 导入库 + DLL 全部通过）。首次触发仍是未解之谜，
`LNK1201` 指向 PDB 写入，怀疑与 Defender 实时扫描有关（未取证）。

## 1. Bug B 的根因被埋点**证实**（不再是推断）

本轮 PIE 日志原文（Role=2 = 本端客户端）：

```
[Input] TankPawn_0 客户端就绪自愈：Tick 注册 0→1 / 启用 0→1，输入组件补建，动作绑定补绑，IMC新挂
```

客户端世界里的本机坦克到达时：**Tick 未注册、未启用、`InputComponent` 根本没建、动作没绑、
IMC 没挂** —— 五项全缺。输入回调（不走 Tick）活着，Tick 侧全死，故「只能开炮不能移动」。
假设 (a) 引擎漏注册 **成立**，(b) WASD 没投递被证伪。

端到端也过了：向 `Tank Preview [NetMode: Client 1]` 窗口 PostMessage 按住 W 1.5s，
该客户端坦克前进约 2670cm，服务器侧代理位置同步，`本机位移首次生效` 恰好 1 条
（只有被驱动的那个客户端）。

## 2. 新 bug：炮塔朝向复制到了、但客户端**落地不了**

用两个可读属性做判定：`current_turret_yaw`（实际写进 `TurretPivot` 的值）与
`net_turret_yaw`（复制进来的权威值）。实验数据（Client1 按 E 前 → 后）：

| 视角 | `net_turret` | `current_turret` | 结论 |
|---|---|---|---|
| Client1 本机 | 0.0（`COND_SkipOwner`，符合设计） | 32.1 → **102.1** | 输入/伺服正常 |
| 服务端看它 | 32.1 → 102.1 | 32.1 → **102.1** | 服务器侧可见 ✓ |
| **Client2 看它** | 32.1 → **102.1** | **0.0 不变** | **复制值到了，`TurretPivot` 不动** ✗ |

**根因**：炮塔落地在 Tick 里（`TurretPivot->SetRelativeRotation`），而**客户端世界里的远端坦克也是
Tick 关** —— 与自己的车同一个病灶（网络生成 ⇒ Tick 未注册），但上一版自愈只覆盖了
「本机玩家的本机 Pawn」（当时的理由是「远端不需要输入」，忽略了远端也需要 Tick）。

注意 Tick 第 8 段本来就写了「其他实例从实际位移反推负重轮转速」的分支，注释明确提到
「客户端的远端坦克」—— 说明作者原本就假定远端车会 Tick，只是这个前提没被满足。

**修法**：`EnsureClientReady()` 拆成两段、两个闸门：

| 段 | 闸门 | 覆盖范围 | 内容 |
|---|---|---|---|
| Tick 自愈 | `bTickRepaired` | 客户端世界里的**每一辆**坦克（含远端） | `IsTickFunctionRegistered()` 为假才补注册，`IsActorTickEnabled()` 为假才补启用 |
| 输入自愈 | `bClientReady` | 只对本机玩家的本机 Pawn | 补建 `InputComponent` / 补绑动作 / 补挂 IMC |

钩子不变：`PostNetReceive()` 由 `FObjectReplicator::PostReceivedBunch()` 在
`bHasReplicatedProperties` 时调用，客户端收到**任何**复制属性变化都会走到，远端车同样如此 —— 可靠。

**复验结果**（PIE 三开，重启后）：

```
每个世界 × 每一辆车：Tick开          ← 修复前客户端世界的远端车是 Tick关
[Net] TankPawn_0 Tick 自愈：注册 0→1 / 启用 0→1（Role=2）    ← 本机自己的车
[Net] TankPawn_1 Tick 自愈：注册 0→1 / 启用 0→1（Role=1）    ← 客户端上的远端车
[Net] TankPawn_2 Tick 自愈：注册 0→1 / 启用 0→1（Role=1）
```

驾驶者炮塔转到 322.5 后，**另一客户端**看这辆车：`current_turret=322.5 / net_turret=322.5` ✓
（修复前这里恒为 `0.0`）。附带收益：远端坦克的负重轮反推（Tick 第 8 段）现在也开始生效。

## 3. 本轮新增/废弃的脚本

| 脚本 | 状态 | 说明 |
|---|---|---|
| `Scripts/pie/pie_turret_report.py` | 新增 | 按世界枚举每辆车的 `current/net_turret`、Tick 开关、归属 —— 炮塔类问题的标准探针 |
| `Scripts/pie/pie_drive_report.py` | 新增 | 同上但看位姿 + 炮塔 + Tick（移动类问题的标准探针） |
| `Scripts/tools/win_list.py` | 新增 | ctypes 列可见窗口（含 hwnd/pid），定位 PIE 客户端窗口。用 ctypes 是因为本机 PowerShell 的 `Add-Type` 被安全策略禁止 |
| `Scripts/tools/win_key.py` | 新增 | 按窗口 PostMessage 投键。`--activate` 先发 `WM_ACTIVATE(WA_ACTIVE)` 让 Slate 把该窗口记为活动窗口（**不抢操作系统前台**）；`--foreground` 才会真抢并把焦点还回去 |
| `Scripts/probe/probe_ia_assets*.py` / `probe_input_routes2.py` / `probe_turret_read.py` | 新增 | API 探针，用于确定「哪些 Python 接口在这台机器上真的可用」 |
| `Scripts/pie/pie_drive_clients.py` | **已删除** | 原打算用 Enhanced Input 注入驱动客户端，实测路线不通（见下），属死路，删掉避免误导 |

## 4. 两处 Python API 硬坑（实证，已写进项目记忆）

- **`unreal.EditorAssetLibrary` 在本工程整个是坏的**：`load_asset` / `does_asset_exist` /
  `list_assets` 对本工程一律返回空（连 `/Game` 都报不存在），但同一次执行里
  `unreal.load_asset("/Game/tank/inputs/IA_MoveForward")` 正常、`AssetRegistryHelpers.get_assets_by_path`
  也能列全 10 个资产。取资产**一律用 `unreal.load_asset`**。
- **Python 无法注入 Enhanced Input**，因此没法纯脚本驱动坦克：
  `ULocalPlayer::GetSubsystem` 是模板（未反射）；`USubsystemBlueprintLibrary` 因
  `BlueprintInternalUseOnly` 没进 Python 绑定（`unreal.SubsystemBlueprintLibrary` 压根不存在）；
  `GameInstance` 没有任何 player 成员；`PlayerController::GetLocalPlayer` / `Player` 也未反射。
  三条探针交叉确认。→ 驱动 PIE 里的坦克只能靠 `win_key.py` 从窗口层投键。

---

# 第六批（2026-09-12）：坡度 / 障碍 / 重力专项排查（只诊断，未改游戏代码）

方法：静态通读 `TankPawn` 的贴地 / 移动 / 坠落三条代码路径；PIE 三开（Listen Server + 2 Client）
用 `win_key` 投键驱动 + 探针脚本逐帧采样。关键基数（M4b 缩放后）：
碰撞盒半尺寸 **190 × 87.5 × 59**；贴地探针窗口 = 盒底向下一共 **140cm**；坡 **40°**（tan40°=0.8391）。

SE 坡实测几何（用于所有判据）：坡面高度 `surface(X) ≈ 2.1 + (X − 674.2) × 0.8391`；
坡顶面在掩体西表面（X=1199.2）处 = **442.5**；掩体（Cover_SE）顶面 = **460** → 接缝差 17.5cm。

## 1. B1（核心）：上坡姿态全程 0° —— 车头啃坡、车尾最大悬空 3.2m

驾驶路径：SE 坡 +X 上坡（客户端本机车，yaw=0，speed=400），10 次连续采样：

| 采样 | 车心 X | 盒底 Z | 探针（起点→终点） | 探针结果 | 前缘离坡面 | 车尾悬空 | 应到 pitch | 实际 pitch |
|---|---|---|---|---|---|---|---|---|
| 1 | 466.7 | 2.4 | 12.2→-137.8 | 命中 2.2 / 法线 1.00（平地） | — | — | 0° | 0° |
| 2 | 630.1 | 125.4 | 135.4→-14.6 | 命中 2.2 / 法线 1.00（打到坡前路面） | 1.0 | 123.2 | +17.8° | 0° |
| 3 | 708.4 | 191.1 | 201.1→51.1 | **未命中** | 1.0 | 188.9 | +26.3° | 0° |
| 4 | 786.6 | 256.7 | 266.7→116.7 | **未命中** | 1.0 | 254.5 | +33.7° | 0° |
| 5 | 864.8 | 322.4 | 332.4→182.4 | **未命中** | 1.0 | 319.8 | +40.0° | 0° |
| 6~10 | 1009.2 ×5 | 443.5 | 453.5→303.5 | **未命中** | 1.0 | 319.8 | +40.0° | 0°（卡死） |

（"前缘离坡面" = 车底前缘角点到正下方坡面的距离；"车尾悬空" = 后缘角点到正下方路面/坡面的距离；
"应到 pitch" = 让盒底前后缘同时贴住各自下方地面的俯仰角）

**根因（三条链同时成立）**：

1. **探针够不到坡面**：探针从「盒底 +10」向下探 140。水平的车在 40° 坡上爬行时，
   坡面在**车心正下方**比盒底低 **190·tan40° = 159.4cm > 140** → 坡中探针必然未命中；
   坡底附近（采样 2）够得着，但打到的是坡前的平路（法线 1.00）→ 对齐目标 = 水平。
2. **实际爬升靠移动 sweep 的滑移**：`AddActorLocalOffset` 撞坡 → `VectorPlaneProject` 沿坡滑，
   位置确实在沿坡上升；而两支贴地逻辑（命中就下移贴地 / 未命中就自由落体）的下移
   都会被**车头下方的坡面**挡停 → 盒底被钉在「车头前缘接触点高度」上（所有采样前缘恰好离地 1.0cm）。
3. **姿态对齐分支永远拿不到坡面法线**：采样 2 拿到的是平路法线（→ 目标姿态水平），
   采样 3~6 干脆走悬空分支（该分支只改位置、不改旋转）→ pitch 全程 0。
   `UpdateGroundContact` 里的对齐数学本身是对的（`TankPawn.cpp:809-815`），**输入的法线从来不是坡面**。

**140 这个值注定不够**：40° 坡上"水平姿态 + 车头前缘贴坡"要求探针窗口 ≥ 190·tan40° ≈ **159.4**，差 19.4cm。

**后果**：车以水平姿态爬坡（前保险杠插在坡里、车尾翘空最多 319.8cm），且最终爬不上去（见 G2）。
证据截图：`Saved/Screenshots/WindowsEditor/ramp_pitch_bug_side5.png`
（一辆车水平悬空漂在 40° 坡上方，旁边路面上有一辆正常停稳的车作对照）。

## 2. G2：卡死在坡顶（B1 的连锁）

采样 6~10 是**五个完全相同的位姿**（`X=1009.2, z=502.5, pitch=0`，按住 W 不再前进）：

- 车头端面 X = 1199.2 = 掩体西表面；车底前缘只到 443.5，而掩体顶是 460 →
  **车头端面与掩体竖面重叠 16.5cm**；
- 阻挡面是竖直面（法线 Z=0）→ 不可行走 → 滑移分支不适用 → 前进被挡死；
- 同时探针（453.5→303.5）未命中（坡面在车心下方 ~160cm）→ 也无法靠贴地/对齐自救；
- 坡顶接缝实际是一个 **17.5cm 台阶**：坡面在掩体面处 442.5 vs 掩体顶 460。
  这是生成脚本 `level_cover_ramps.py` 的 `OVERLAP=20`（坡顶面伸进掩体 20cm 消缝）的副作用：
  20·tan40° ≈ 16.8cm 的落差恰好变成台阶；
- `MaxStepUp`（=15）全工程无消费点 → 台阶逻辑从未实现；即使实现了，15 < 17.5 也不够。

## 3. G3：坠落的最后 131cm 被贴地逻辑"吸收"

从掩体东侧边缘开下去（自由落体）：

```
X=2283.3  盒底 351.3  探针 361.3→211.3 未命中   → 正常下落
X=2416.7  盒底 133.5  探针 143.5→-6.5  命中 2.2  → 盒底将摆到 2.2（Δ=-131.4）
X=2550.0  盒底   2.2  已落地
```

盒底进入 140 窗口的瞬间被判为"着地"，一帧内把盒底从 133.5 摆到 2.2 —— 最后 131cm 的坠落
（按此刻 ~8m/s 的落速，正常要十几帧）被压成一帧：视觉上是"提前贴地 + 一帧下坠 131cm"。

## 4. 下坡：坡中跟随良好，坡底"翻平 + 一帧竖直拽"

从 SE 掩体顶朝 -X 下坡（yaw=180）：

| 采样 X | pitch | 探针 | 说明 |
|---|---|---|---|
| 895.7 | **-40°** ✓ | 命中 187.9 / 法线 0.77，Δ=-19.0 | 姿态正确跟随（Δ≈59/cos40°-59≈18cm 正是"贴合坡面"的稳态跟随值） |
| 691.4 | **-40°** ✓ | 命中 16.5 / 法线 0.77，Δ=-94.7 | 车头端已出坡底（离路面 0.7）、车尾仍在坡上方 76.7，过渡瞬间 |
| 589.3 | **-0°** ✗ | 命中 2.2 / 法线 1.00，Δ=**-109.1** | 车心刚过坡底 → 探针打到平路 → 对齐目标翻成水平，整车从 -40° 被掰回 0°，同时一帧下拽 109.1cm（此刻车头悬空 109.1、车尾离坡面 20.9）→ "砸地"感 |

根因与 B1 同源：**单点中心采样**。车心在"坡上 / 路面上"的切换让 40° 姿态瞬间失去依据。

## 5. B5：Ramp_SW_Y 坡底悬空 43cm，且与邻居坡底对撞（已定位到生成脚本）

6 块坡的坡底体检（`Scripts/probe/probe_ramp_bases_all.py`，坡底端面 Z 的设计值 = 路面 2.2 − SINK 20 = -17.8）：

| 坡 | 坡底端面 Z | 判定 |
|---|---|---|
| Center_R / NE / NW / SE / Center_L | -17.8 | ✓ 5 块全部正确埋入 20cm |
| **Ramp_SW_Y** | **+45.2** | ✗ **坡底悬空 43cm**（应 -17.8） |

**根因**（`Scripts/level/level_cover_ramps.py:95-99`）：坡底高度用「坡底位置竖直探针」实测，
**探针没有 ignore 列表**。SW_Y 的探针位置（Y≈3949.5）正好落在邻居 **Ramp_Center_L_Y 的坡面**上
→ 测到 65.6 而不是路面 2.2 → `base_z = 65.6 − 20 = 45.6` ≈ 实测 45.2 ✓。

x=-1600 纵剖复验（`Scripts/pie/pie_sw_y_crosssection.py`）：

```
Y=3860  hitZ=57.23  normalZ=0.77  <Ramp_SW_Y>        ← SW 坡面继续下降，3870 处 48.84 到边
Y=3880  hitZ= 6.89  normalZ=0.77  <Ramp_Center_L_Y>  ← 已换到邻居坡面，继续上升
```

两块中心走廊坡的坡底在 Y≈3874 处**头对头**（footprint 重叠 3851~3874），SW 的坡底端面悬在
Center_L 坡底上方 43cm。**后果**：车从南往北开中心走廊，会在 Y≈3874 撞上一堵 46cm 高的悬空竖面
（SW 坡的坡底端面，网格 Z 最低点 -0.8 ~ 顶面 45.2）。

## 6. 静态代码问题

| 编号 | 位置 | 问题 |
|---|---|---|
| B3 | `TankPawn.h` `MaxStepUp=15` | 声明后**全工程无消费点** → 台阶逻辑从未实现 |
| B4 | `TankPawn.cpp` `bGrounded` | 只写不读（无 UPROPERTY、无消费者）→ 对外不可见 |
| B6 | `TankPawn.cpp:802` | 贴地量用**未旋转半高**（`loc.Z − HalfHeight`）。40° 时盒的真实最低点比车心低 `190·sin40°+59·cos40° = 167.3`（而非 59）→ **一旦修好探针（B1），车会被按 center−59 按下 108cm 埋进坡里**。修 B1 前必须一起处理 |

## 7. 确认正常的部分（回归基线）

- 平地贴地/对齐完全正常（采样 1：探针 Δ=-0.2，四角间隙 ≤0.2cm）；
- 6 块坡里 5 块的坡底埋入 20cm，上下坡衔接无坎（除 SW_Y）；坡面在 X≈674 穿过路面（2.2）后埋入地下；
- 边界挡车有效：从掩体上落下后一路向东开，车头停在 **X=3447.9**（Bound_East 内表面 3449）；
- **但 `docs/STATUS.md` 的边界数字已过时**：文档写 `X∈[-2969,2969]、高 100`，
  实测 `Bound_East` 内表面 X=3449、高 242（Z -111~131），差 480cm（按构造批次 4 的旧值）。

## 8. 建议修法方向（未实施，按优先级）

1. **多点地面采样**（一并解决 B1 和下坡坡底翻平）：沿行驶轴 ±190 各打一条竖直探针，
   用两点高差反推 pitch/roll；或保留单点但把「命中法线」用起来（当前只有中心命中才算数）。
2. **贴地量改用旋转后的最短半高**（`hx·|sinθ|+hz·|cosθ|`）或按四角最低点贴地（B6）。
3. **探针窗口 140 → >160**（40° 坡硬下限 159.4），或改成"从车心向下 400+"。
4. **坡顶台阶**：`level_cover_ramps.py` 的 `OVERLAP=20` → 0（坡顶面正好压在掩体面，接缝齐平），
   或把掩体顶降 17.5cm；若保留台阶则必须实现 `MaxStepUp` 且值 ≥17.5。
5. **SW 坡底**：生成脚本的坡底探针加 ignore 列表（排除 `Ramp_*`）后重测地面；重生成后应落到 -17.8。
6. **文档**：`docs/STATUS.md` 的边界数字按实测更新。

## 9. 本轮新增脚本 + 一个 Python API 陷阱

| 脚本 | 用途 |
|---|---|
| `Scripts/pie/pie_m4c_ramp_drive_setup.py` / `pie_m4c_ramp_sample.py` | 坡上行驶逐帧采样（客户端本机车：C++ 探针复现 + 四角间隙 + 理想姿态） |
| `Scripts/pie/pie_sw_y_crosssection.py` | SW / Center_L 坡底纵剖 |
| `Scripts/probe/probe_ramp_bases_all.py` | 6 块坡坡底体检 |
| `Scripts/pie/pie_m4c_side5_setup.py` + `pie_m4c_side5_shot.py` | "车悬空在坡上"取证截图 |
| `Scripts/pie/pie_rotator_order.py` / `pie_cam_probe.py` | Rotator 参数序复验 / 视口朝向探针 |

**陷阱（实证）**：本引擎 Python 的 `unreal.Rotator(1,2,3)` 位置参数序是 **(roll, pitch, yaw)**，
不是文档写的 (pitch, yaw, roll)：`Rotator(1,2,3)` → `pitch=2 yaw=3 roll=1`。
**必须关键字传参**（`level_cover_ramps.py:123` 早有此注释；本轮取景脚本又踩了一次：
想设 yaw=90 实际设成了 pitch=90，前几张"侧视图"机位全部拍错方向，直到用
`pie_rotator_order.py` 复验参数序才定位）。

---

# 第七批（2026-09-12 晚）：按体感三问题修复 —— 坡道固定朝外 + 接缝齐平 + 四角贴地

用户报的三条（原话）：1) 斜坡与障碍物的接缝不自然；2) 爬坡时浮空、重力不对，此时后撤直接浮空然后落体；
3) 两个坡把中间道路挡住 —— **不要随机方向，给每块障碍物固定一个方向，衔接自然**。

## 0. 改了什么

| 症状 | 根因 | 改法 |
|---|---|---|
| 接缝 17.5cm 竖坎（G2 卡死在坡顶） | 旧脚本 `OVERLAP=20` 让坡面伸进掩体 20cm，坡面在掩体面处因此比顶面低 20·tan40° | `OVERLAP=0`：坡面正好交在「掩体表面 × 顶面」棱上 |
| 两块坡顶头堵死走廊/中路 | 方向随机，同列相邻掩体的坡会朝对面长 | 方向写死**朝场外**：左列 `-X`、右列 `+X` |
| 坡底悬空 43cm（B5） | 坡底探地没 ignore 邻居坡 | 坡底边**三点探地取最低者**再下沉 20；探针 ignore 全部 `Ramp_*`/`Cover_*` |
| 上坡全程 0°、车尾悬空 3.2m（B1） | 单点中心探针在坡上必然打空 / 打到坡前平地 | `UpdateGroundContact` → **前/后 × 左/右四角采样**，`atan2` 反推 pitch/roll |
| 贴地量用未旋转半高（B6） | —— | 车心到坡面竖直距离 = **半高 / cos(倾角)** |
| 坠落最后一帧瞬移 131cm（G3） | 探针一进 140 窗口就整段贴下去 | 坠落中（V < -50）下探窗口只给「本帧落体步长 + 5」 |
| `MaxStepUp` 从未被消费（B3） | —— | 现有两个消费点：贴地带的上限 + 采样时排除「高出车角 MaxStepUp 的台面」 |
| `bGrounded` 只写不读（B4） | —— | 加 `UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)`，PIE 探针可直接读 |

坡度 40° → 30°：坡不再需要挤走廊（朝场外，坡底离边界还有 300+cm），30° 更自然，也给姿态对齐留足收敛余量。
坡宽按**掩体实际面宽**算（角掩体 800×800 → 780；中掩体 1400×400 → 380）。

## 1. 新坡几何（实测，6 条坡底全部 -17.8）

测法：`probe_ramp_layout_audit.py`（AABB 审计）+ `probe_ramp_seam.py`（沿坡轴线纵剖）。

| 坡 | 方向 | 坡高 | 水平投影 | 斜面长 | 宽 | 坡底坡面 Z | 顶边 Z |
|---|---|---|---|---|---|---|---|
| Ramp_Center_L_X / _R_X | 朝西 / 朝东 | 478 | 828 | 956 | 380 | -17.8 | 460 |
| Ramp_NW_X / NE_X / SW_X / SE_X | 朝西 / 朝东 | 478 | 828 | 956 | 780 | -17.8 | 460 |

- **接缝纵剖**（Ramp_SE_X 中轴，25cm 一步）：`road_hd 2.2` → 恒定 30° 坡面（每步 +12.5）→ `Cover_SE 460.0`；
  全程只升不降（末段 +2.8 是采样步长效应，坡面在顶边棱处正好 460）✓
- **坡底**：坡面 9.7 → 路面 2.2 连续（坡面在坡底边前 ~35cm 处没入路面以下）✓ 无坎无悬空
- **布局审计**：坡×坡 **0 相交**；6 对坡×掩体只在「掩体顶面棱」处相接（朝中路仅多出 30cm = 顶边厚度，藏在掩体内）；
  **每条坡的 Y 范围 ⊆ 自己掩体的 Y 范围** → 走廊（掩体之间的 Y 空档）与中路（|X| < 900）里没有任何坡 ✓

## 2. 物理修复实测（PIE 三开，投键驱动客户端车，400cm/s）

`gap` = 车体**真实四底角**正下方地面高度 − 角点高度（正 = 插进地面 / 负 = 悬空）；`Δz` = C++ 贴地目标 − 当前高度。

**上坡（W）**：

| t | 车心 X | pitch（实测 / 理想） | Δz | 四角 gap |
|---|---|---|---|---|
| 17.00 | 3150（平地） | 0.0 / 0.0 | -0.2 | -0.2 |
| 17.67 | 2908（坡底跨接） | +5.9 / +5.9 | -24.7 | -17.6 ~ -24.4 |
| 18.33 | 2670（姿态收敛中） | +25.9 / +25.9 | -38.0 | -36.4 ~ -55.0 |
| 19.00 | 2434（坡中） | **+30.0 / +30.0** | -1.0 | **-1.0** |
| 19.33 | 2319（坡中） | **+30.0 / +30.0** | -0.1 | **-0.1** |
| 21.00 | 1702（掩体顶） | 0.0 / 0.0 | -5.7 | -5.7 |
| 21.67 | 1436（掩体顶） | 0.0 / 0.0 | -0.0 | **-0.0** |

对照第六批 B1（修复前）：上坡全程 `pitch=0`、车尾悬空 123→319.8cm、最后卡死在坡顶（G2）。
现在：**pitch 与地面反推值逐样本相等**，稳态四角 gap ≤ 1cm，一路开到掩体顶。

**倒车下坡（S，从掩体顶倒回坡面）**：pitch 全程 +30.0（= 理想），四角 gap **0.0**，到坡中停住（S 松开）✓
——"后撤直接浮空然后再落体"的直接对照：修复后倒车全程贴坡、不离地、不落体。

**掉下掩体西缘**：`G=False`（真自由落体，跨多个采样）→ 落地 gap 1.1 → 0.1 ✓
（不再是"探针一进窗口就整段贴地"的 131cm 单帧瞬移 G3）

**跨接过渡**（坡底/坡顶的 0.3~0.5s）：四角 gap 10~50cm —— 刚性单平面模型的必然（前角先贴、后角还在旧面上）；
姿态收敛后立刻回到 ≤1cm。要连过渡也压到 0 需要底盘双接触点模型（见 §4）。

**复制**：服务端那份代理 pitch 同步（15.9 → 25.9 → 30.0），远端视角现在能看到车体俯仰 ✓

## 3. 两个把车"飘"起来的公式坑（已写进代码注释）

1. **坡度要用 `atan2`，不是 `asin`**：四角探针是**水平**偏移 ±(半长−10)，地面高差满足 `tanθ = ΔZ / (2L)`。
   写成 `asin(ΔZ/2L)` 会把 30° 坡算成 `asin(tan30°) = 35.3°` —— 姿态一旦超过坡角，车就**以后角为支点翘头**、
   前部整段悬空（实测前角浮 23~40cm，第四批的"浮空"体感有它一份）。
2. **车心到坡面的竖直距离是 `半高 / cos(倾角)`**：车心沿坡面法线离地半高，法线本身斜了 θ，竖直投影要除 cosθ。
   写成 `半高·cosθ` 在 30° 坡上少算 18cm → 车永远"飘"在坡面上方 18cm（实测跨接带 Δz 恒 -18，且下贴被前角接触扫掠挡住）。

## 4. 仍未解决 / 待观察

- **倒车跨过坡顶时出现过 ~2s 停顿后自行恢复**（实测 t=80.3~82.3 位姿完全冻结，之后继续倒下来）。
  发生在"车尾压在坡顶棱、车头还在掩体顶"的桥接姿态；**前进方向跨顶正常**。未深挖，记录待观察。
- **俯视布局截图没拍成**：`LevelEditorSubsystem.set_level_viewport_camera_info` 需要 `viewport_config_key`，
  编辑器自动化截图拍的是当前活动视口 → 布局改用 AABB 数值审计（§1 第 3 条）。
  另注：**PIE 运行时编辑器世界取不到几何**（`get_all_level_actors()` 返回 0、编辑器世界的探针全未命中），
  关卡侧探针要等 PIE 停止后再跑。
- 跨接过渡期 gap 10~50cm（§2 末）—— 彻底消除需底盘双接触点/多点接触模型。

## 5. 本轮脚本与截图

| 资源 | 用途 |
|---|---|
| `Scripts/pie/pie_m4c2_setup.py` | 把 Client1 车摆到坡东侧起跑线（3150,3000）朝 -X、限速 400；主机车挪到西北角 |
| `Scripts/pie/pie_m4c2_sample.py` | 一行式采样：真实角 gap + C++ 复算（理想 p/r、Δz、是否在接触带）+ 服务端代理 pitch |
| `Scripts/pie/pie_m4c2_place.py` | 读 `Scripts/_place.txt` 摆位，并做箱体向下扫掠诊断（判断贴地被谁挡住）|
| `Scripts/pie/pie_m4c2_midslope.py` | 坡中段静态贴地测试 |
| `Scripts/pie/pie_m4c2_shot_stage.py` / `_shot_take.py` | 读 `Scripts/_shot.txt` 摆三辆车 / 调 `tank_shot` 截图 |
| `Scripts/probe/probe_ramp_seam.py` | 坡-掩体接缝纵剖（沿坡中轴 25cm 一步，带命中件名）|
| `Scripts/probe/probe_ramp_layout_audit.py` | 布局 AABB 审计（坡×坡 / 坡×掩体 / 走廊与中路 / 坡 Y ⊆ 掩体 Y）|
| `Scripts/probe/probe_hit_tuple.py` | `FHitResult.to_tuple()` 索引表 + 掩体实际尺寸实测 |

截图（`Saved/Screenshots/WindowsEditor/`）：
- `m4c2_climb_side.png` —— **修复后**侧视：车以 30° 姿态贴坡爬升，旁边路面上有一辆水平车对照
- `m4c2_crest_junction.png` —— 坡顶接缝特写（车在坡上 + 车在掩体顶，坡面与顶面齐平）
- `m4c2_corridor_clear.png` —— 原 B5 撞头点（左列走廊 Y≈3874）：现在能站车、无坡挡路
- 对照：`ramp_pitch_bug_side5.png`（第六批，修复前：车水平悬空漂在坡上方）

**FHitResult 索引（本引擎实证）**：`to_tuple()` → `[0]` bBlockingHit、`[1]` bStartPenetrating、`[2]` Time、
`[3]` Distance、`[4]` Location、`[5]` ImpactPoint、`[6]` Normal、`[7]` ImpactNormal、`[8]` PhysMaterial、
`[9]` 命中 Actor、`[10]` Component、`[11]/[12]` BoneName/MyBoneName。
（`probe_ramp_bases_all.py` 用 `[11]` 取 actor 只能拿到 `?`，就是这个原因；另：探针起点必须高于目标面，
否则会"命中自己的起点"在 Time=0 处读到起点 Z —— 本轮排查时踩过一次。）

---

# 第八批（2026-09-12 深夜）：玩家截图「半个身子插进土里」+ 掩体降高 + 俯角拓宽

玩家反馈（附截图）：主机那辆车半个身子插在掩体里、姿态歪成怪角度动不了；另提两条需求：
掩体顶太高打不到地上的车（要拓宽俯角、且不能被炮管切进车体）、掩体高度降到原来的 2/3。

## 1. 「插土」根因（取证 = `Scripts/pie/pie_burial_probe.py`）

对现场那辆车直接测：

```
loc=(-1179.2, 4590.5, 519.2)  p/y/r=(-51.8, -54.7, 55.9)
真实角 gap: 左前+65.8 右前+155.4 左后-232.9 右后-143.3（正=角点插进地面）
零长度箱体扫掠: 命中 <Cover_Center_L> StartPenetrating=True；抬起 ≈160cm 才不阻塞
```

机制（三条链）：

1. **采样没有下界**：车头悬在掩体边缘外时，车头探针把下面 **458cm** 处的路面当成"坡面"采样
   → pitch 目标 = `atan2(2.2-293.3, 360)` = **-39°**（连 roll 也被带飞）；
2. **姿态旋转没有扫掠**：`AActor::SetActorRotation` 默认不做扫掠 → 车头直接被转进掩体顶面（埋深 155cm）；
3. **埋进去后自锁**：角点采样因"地面在角点上方"被 `MaxStepUp` 过滤全灭 → 只剩"保持当前姿态"的单侧分支
   → 靠自己永远出不来（`bGrounded` 还是 True）。

## 2. 四处修法（都在 `TankPawn::UpdateGroundContact`）

| # | 修法 | 关键代码 |
|---|---|---|
| 1 | 采样加上界 + **下界** | 上界：`Z > CornerBottom + MaxStepUp` 排除（台面）；下界：`Z < CornerBottom - MaxSampleDrop` 排除（台缘外地面），`MaxSampleDrop = 2·ProbeLong·tan(MaxClimbSlope) + 2·MaxStepUp` |
| 2 | **姿态旋转带扫掠** | UE5.8 的 `AActor::SetActorRotation` 无 `bSweep` 重载 → `CollisionBox->MoveComponent(零位移, 新旋转, bSweep=true)` |
| 3 | 姿态目标**夹到最大爬坡角** | `Clamp(TargetPitch/Roll, ±MaxClimbSlopeDeg)` |
| 4 | **脱困兜底** | `DepenetrateIfStuck()`：内缩 1cm 箱体 `OverlapBlockingTestByChannel`，阻塞就逐级抬起（24cm×12 步；更深的埋体分帧抬），Log 留痕 |

顺带修「骑棱悬空冻结」：自由落体蹭到棱/竖面（法线不可行走）**不是支撑** → 沿接触面切向继续滑落
（否则车头搭在掩体棱上、尾浮 223cm、姿态一动不动）。

**另一个真 bug（诊断途中抓到）**：客户端坦克的 **Tick 会在运行中再次丢失**（World 时钟照走、车一动不动；
投一次键触发复制后 `Tick 自愈：注册 0→1` 才回来）。原来自愈只挂在 BeginPlay/复制/占有钩子上，没有网络流量就补不到
→ 加 **1s 心跳定时器** `RepairTickTimer()` + 去掉 Tick 自愈的一次性闸门（开关判断本身很便宜，每次查）。

## 3. 掩体降高 2/3 + 坡道重生成（`Scripts/level/level_cover_shorter.py`）

总高 500 → 333.3（顶面 460 → 293.3），底面仍压地板 -40。**立方体是中心锚点：只改 Z 缩放必须同时重算中心 Z**
（中心 = 地板顶面 + 新半高 = 126.67），否则底面浮空 —— 第一版脚本就踩了（缩放改了、定位 API 参数错，掩体悬空 43cm）。

重跑 `level_cover_ramps.py`：坡高 478 → **311**、水平投影 828 → **539**、斜面 622、坡度仍 30°、顶边 Z=293（对齐新顶面）
坡底仍全部 -17.8 ✓。接缝纵剖复验：`road 2.2 → 30° 坡面 → 293.3` 只升不降 ✓。

## 4. 俯角拓宽（-5° → -14°，不切车体）

PIE 几何实测：炮轴心离地 **87**、炮管世界伸出 **228**（456×0.5）、炮管越过车体前缘 **105**、车体甲板高 **~60**
→ 不切进车体的极限 ≈ `atan((87-60)/105) ≈ 14°`。
-5° 时从掩体顶（炮轴心离地 ~380）只有 34m 外的地面目标能打到；-14° 后死角半径 ≈ **15m**。
想彻底解决"打脚底下的车"只能靠弹道下坠或开下掩体（真坦克前向俯角本来也只有 -6°~-10°）。

## 5. 验证（PIE 三开 + 投键驱动）

- **埋体恢复**：把车摆进掩体内部（150）→ 自动抬出并落到掩体顶（车底 293.3、四角 gap 0.0）✓
- **上坡（新坡）**：pitch 与地面反推值逐样本相等（+4.4 → +24.3 → **+30.0**），掩体顶 gap 0.0 ✓
- **开出掩体边缘**：pitch -39（悬挂姿态）→ 真自由落体 → 落地 gap 0.0 ✓ 全程 `max穿模 0.0` ✓
- 取证截图：`m4c3_shorter_covers.png`（新掩体高 + 坡 + 爬坡车）、`m4c2_climb_side.png`、`m4c2_crest_junction.png`

## 6. 仍未解决 / 待观察

- **跨接过渡**（坡底/坡顶 0.3~0.5s）四角 gap 10~50cm：刚性单平面模型的必然，要消掉需逐点弹簧模型（Chaos 载具或手写四轮射线悬挂）；
- **人工摆进"骑棱"姿态后会悬空冻结**（无穿模、可开动脱困）：`Δz` 出带、但落体扫掠被棱的接触挡住 → 已加"沿切面滑落"，实测该姿态下仍未自行滑脱（未定位到最后一环：疑似落体位移被判 Time=0），记录待查；
- 倒车跨坡顶时出现过约 2s 停顿后自恢复（前进方向正常）。

本轮新增脚本：`pie_burial_probe.py`（埋体/接触/炮管几何）、`pie_m4c2_place.py`（可参数化摆位 + 箱体扫掠诊断）、
`pie_m4c3_gun.py`（俯角读写与炮口位姿）、`level_cover_shorter.py`（掩体降高）。








---

# 第九批（2026-09-12 深夜二）：路线 2 落地 —— 逐点接触姿态 + 炮塔转速翻倍

玩家第二张截图（车在坡底附近侧倾 30~40°、一侧履带悬空）+ 明确要求：先试路线 2、把鼠标炮塔转速加到两倍。

## 1. 逐点接触（路线 2 的静态版）—— 采样 4 点 → 8 点，姿态只由「真正受力的点」决定

四角采样换成**四角 + 四边中点共 8 点**，并把每组的姿态平均换成**接触内点筛选**：
组内先找离车底最近的采样（最小浮空高度 = 接触候选），比它高出 `max(2·MaxStepUp, 30cm)` 以上的点**不参与平均**
—— 那是另一块面，不是这一组脚下的地。骑棱 / 压半边坡时，正是这些「另一块面」的采样把姿态拉歪的。

| 参数 | 值 | 说明 |
|---|---|---|
| `MaxContactDrop` | 220cm | 采样下界（新，替换原来按 45° 推的 390）：30° 坡上水平姿态时车尾探针落差 208cm 是合法的，掩体/边界落差 240~293 必须被拦 |
| `MaxGroundRollDeg` | 15° | 姿态侧倾上限（新）：本作场地没有横坡，roll 只可能来自骑棱；真坦克侧倾容忍也就 15° 上下 |
| 单侧有地分支 | 姿态**朝水平缓释**（原为死抱当前角度）| 骑棱前悬时不再死抱 -39° 出不来 |

## 2. 实测（PIE，探针脚本）

| 场景 | 改前 | 改后 |
|---|---|---|
| 压半边坡（左履带在坡面、右侧在路面） | roll 目标 37°、被带翻 | **roll 夹到 -15°**、左前/左后角 gap -0.3（真接触）、整车稳定 ✓ |
| 骑棱前悬（车头悬在掩体内缘外 291cm） | pitch 死抱 -39°、悬空不动 | **pitch 立即缓释到 0**、车底 352.3 = 掩体顶 + 59、后角 gap 0.0（趴在棱上）✓ |
| 上坡（回归） | pitch 逐样本等于理想 | **不变**：+4.4 → +24.3 → +30.0 → 顶面 gap 0.0 ✓ |
| 全程穿模 | — | `max穿模 0.0` ✓ |

## 3. 鼠标炮塔转速 70 → 140（`TurretRotateSpeed`）

原值 70 是"2× 灵敏度"的保守取值，手感上炮塔总慢半个身位；现在与鼠标偏航同速，甩炮塔不用等。
（`CameraSensitivityX` 没动 —— 鼠标到视口的响应本来就是即时的，慢的是炮塔伺服追赶视口的这一段。）

## 4. 遗留（路线 2 的下一步）

现在的实现是「逐点接触的**静态**版」：姿态由接触点集合直接解出，没有力的积分。真要做「重心越过支撑才翻」「侧坡滑落」
这类行为，需要把静态解换成**逐点弹簧-阻尼 + 车体 3 自由度（z/pitch/roll）微动力学**；本次先验证静态版够不够用。

---

# 第十批入口（2026-09-12）：载具升级立项

玩家决定把手写地形跟随换成 **UE5 标准载具（Chaos Vehicles）**。完整计划见
**`docs/tank-vehicle-upgrade-plan.md`**（现状/目标、引擎源码盘点、四阶段计划、风险与回退、验收清单）。

要点速记：
- 硬前提：Chaos 载具的轮子靠 `FChaosWheelSetup::BoneName` 定位 → **坦克必须先做成骨骼网格**（现为 12 个静态网格）；
- 引擎**没有**坦克差速转向支持（`grep Tank|Tracked` 零命中）→ 按侧驱动力矩要自己写；
- 与 M1 的「客户端权威 50Hz 上报」**互斥**（载具组件是服务器权威 + 客户端预测）→ 联机移动同步要重做，
  这是最大的一笔返工；`ATankPawn` 与 `UpdateGroundContact` 全程保留以便回退。
- 开工前已提交当前版本：`bc8cd98`。

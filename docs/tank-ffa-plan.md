# 坦克 FFA 联机对战计划书（2~8 人个人战）

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
- 胜利条件：先到 K 杀（如 10 杀）；限时排名制列为可选
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
- 计分：`APlayerState::Kills`（引擎自带复制）——每人一份挂在自己的 PlayerState 上，天然多人扩展，GameMode/HUD 遍历 PlayerArray 即可
- 击杀 → 死亡表现 → 3s 重生；**重生短暂无敌 2s**（FFA 混战防落地秒杀，灰盒用闪烁材质表达）
- 胜利：先到 K 杀（默认 10，可调）；GameMode 结算 → 全员胜利面板 → 倒计时重开

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
- PlayerState.Kills 计分、先到 10 杀胜利、回合重置、重生保护
- HUD：本机血条+装填、计分板（Tab 呼出或常驻角落）、死亡/重生提示、胜利面板
- **DoD**：三车完整一局 3~5 分钟，计分板实时正确，胜负面板与重开正确

### M4 打磨与包装（弹性）
- 地图 FFA 化：环形出生点、中央/四角掩体布置、手感参数平衡
- （可选）Windows 打包 + 局域网真机试玩（3 台机器最有说服力）
- （stretch）服务端权威移动 + 预测回滚
- **DoD**：给面试官的 5 分钟演示脚本（开三个客户端：移动同步→混战互射→抢击杀→计分板变化→胜利）

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

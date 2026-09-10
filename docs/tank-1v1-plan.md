# 坦克 1v1 联机对战计划书

> 2026-09-10 定稿。丧尸线冻结于 `zombie-horde` 分支（fc234f0），本文件是坦克 PvP 方向的完整计划。
> 定位：简历项目。国内 UE 岗位以网游为主，网络同步是 gameplay 程序岗刚需而非加分项。
> 后续阶梯：本项目学到的复制/RPC/权威模型是 GAS 技能游戏的全部前置知识，一点不浪费。

## 一、为什么坦克对战好做（可行性结论）

| 维度 | 坦克炮弹对战 | 对比：近战动作对战（永劫式） |
|---|---|---|
| 实体规模 | 2 辆坦克，复制压力趋近于零 | 同左，但状态机复杂度高 |
| 命中判定 | 炮弹延迟宽容：服务端跑弹道物理，命中结果回传即可 | 近战需要延迟补偿/回滚，netcode 最难题目 |
| 时序窗口 | 无 | 振刀/连招取消靠帧级判定 |
| 移动同步 | Pawn 的 ReplicatedMovement 引擎现成 | CharacterMovement 自带预测但要理解 |

**结论：是联机入门的最优项目。** 真正要自己搭的只有两块：自定义移动的输入上报、开火 Server RPC。

## 二、范围（明确做 / 明确不做）

**做：**
- Listen Server 拓扑（主机=服务端+玩家1，玩家2 连入），PIE 双客户端开发调试
- 服务端权威开火与伤害（Server RPC + 服务端炮弹 + 复制血量）
- 移动同步：M1 客户端权威快速闭环；服务端权威+预测列为 stretch
- 击杀 / 重生 / 回合重置闭环
- 双端正确的 HUD（血条、装填、击杀数）
- 验收：双机 PIE 全流程 5 分钟可演示，可打包局域网试玩（可选）

**不做（写下来防蔓延）：**
- 大厅/匹配系统、账号体系
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
| `DefGameMode` | `BattleGameMode` | 换胜负规则：击杀计数 + 回合重置；按 PlayerController 分配出生点 |
| `DefHUD` | `BattleHUD` | Canvas 直绘模式保留，数据源从城墙 HP 换成双方坦克 HP |
| `TankProjectile` | 保留 | 删 `#include "Zombie.h"` 与溅射遍历（TankProjectile.cpp:3,87-99）；命中改对坦克 ApplyPointDamage |

**移除（M0 执行，涉及删除逐一确认）：**
- `Zombie/ZombieSpawner/ClimbManager/DefWall`（分支已存档）
- 地图中的 SmokeWall_Proxy 代理墙、刷怪器、残留丧尸
- smoke_*.py / probe_*.py / fix_proxy_*.py（丧尸专用调试脚本，分支可查）
- 碰撞通道约定重审：ECC_GameTraceChannel1 原是丧尸通道，坦克间改为互相 Block（PvP 要挡住对方）+ 炮弹对坦克 Block

## 四、网络架构决策

### 拓扑：Listen Server
- 开发期：PIE Net Mode = Play As Client 双开（Project Settings → PIE → Run Under One Process + 2 players），或独立双进程连 localhost
- 打包期（可选）：打包 Windows 包，主机开 LAN 房，第二台机器直连 IP

### 移动同步：先客户端权威，服务端权威列为 stretch
- **M1 客户端权威**：客户端本地模拟（现有 Tick 移动不动），通过 `Server RPC` 上报 Transform，服务端校验后转发给另一端（SetReplicateMovement 或手动 ReplicatedTransform）
- 理由：TankPawn 是自定义 Tick 移动（非 CharacterMovementComponent），没有免费的移动预测；客户端权威一天能闭环，演示完全够用
- 面试价值：能讲清"客户端权威 vs 服务端权威"的作弊面/手感 trade-off，比直接抄预测代码更加分
- **stretch（M5+）**：服务端权威移动 + 客户端预测 + reconciliation——做完它就是面试场上的降维打击，但不阻塞主线

### 开火与伤害：服务端权威
- 客户端开火输入 → `Server Fire(cosmetic params)` RPC
- 服务端生成炮弹（服务端模拟弹道 180m/s + 重力），Multicast 播放开炮特效/后坐力
- 炮弹命中在服务端判定：`ApplyPointDamage` 打坦克 → `TankHealth`（Replicated）扣血
- 血量变化用 `OnRep_Health` + 多播委托驱动 HUD；死亡判定在服务端，重生走 GameMode RestartPlayer

### 出生与回合
- GameMode 按 PlayerController 顺序分配对角出生点（地图两端）
- 击杀 → 死亡坦克爆炸表现（P3 灰盒用击飞+缩回）→ 3s 重生；先到 K 杀获胜（如 5 杀）
- 回合重置逻辑参考现有 DefGameMode 的 EndMatch/倒计时重开骨架

## 五、里程碑（每个里程碑结束 = 一个可玩状态）

### M0 清理与地基（约半天）
- 移除丧尸系统与地图残留（清理清单见上）
- `TankHealth` 组件 + 挂到 TankPawn；坦克间碰撞改 Block
- `BattleGameMode` 骨架：多 Pawn 出生（PIE 双开能各开一辆）
- **DoD**：双开 PIE，两辆坦克各自受控、互撞互挡

### M1 移动同步（1~2 天）
- Server RPC 上报 Transform + ReplicatedMovement
- Owner/非 Owner 视角确认：本机即时、远端平滑（NetworkSmoothingMode 调参）
- **DoD**：双开各自开坦克，对端位置/朝向/姿态同步无瞬移

### M2 战斗闭环（2~3 天）
- Server Fire RPC + 服务端炮弹 + Multicast 特效（炮口焰/后坐力本机表现已有）
- 服务端命中扣血、OnRep 驱动 HUD、死亡击飞表现、重生
- **DoD**：双开互射，扣血→死亡→重生全链路，血条双端一致

### M3 回合与 HUD（1~2 天）
- 击杀计数、先到 5 杀胜利、回合重置
- HUD：双方血条、击杀数、死亡/重生提示、胜利面板
- **DoD**：完整一局 3~5 分钟，胜负面板与重开正确

### M4 打磨与包装（弹性）
- 地图对称出生点/掩体布置、手感参数平衡
- （可选）Windows 打包 + 局域网真机试玩
- （stretch）服务端权威移动 + 预测回滚
- **DoD**：给面试官的 5 分钟演示脚本（开两个客户端：移动同步→互射→击杀→胜利）

## 六、风险与坑（预判）

| 风险 | 对策 |
|---|---|
| Live Coding DLL 锁 | 构建前关编辑器（已有标准流程） |
| PIE 双开调试混乱 | 日志按 LogNet 过滤；每端用不同 PIE 窗口名；探针脚本区分 world（PIE 期 get_editor_world()=None 的坑已知） |
| 客户端权威下远端抖动 | NetworkSmoothingMode + ReplicatedMovement 的插值；必要时手动缓冲插值 |
| 炮弹双端生成导致双重伤害 | **只在服务端 Spawn 炮弹**，客户端零炮弹逻辑；特效走 Multicast |
| 输入在非 Owner 端误触发 | SetupPlayerInputComponent 只在 Possessed 端生效（引擎保证），但 Tick 里坦克轮子等视觉更新要确认非 Owner 端也跑 |
| 一轮只动一个变量铁律 | 每个里程碑内一次只改一个系统；M1 没验证完不碰 M2 |

## 七、面试话题映射（每个实现点对应高频面试题）

| 实现点 | 面试可答 |
|---|---|
| Role/Authority 判定 | 客户端/服务端/独立端各自跑什么逻辑 |
| GetLifetimeReplicatedProps | 属性复制条件（COND_OwnerOnly 等） |
| Server RPC vs Multicast | 为什么开火走 Server、特效走 Multicast、可靠性级别怎么选 |
| ReplicatedMovement | Pawn 移动同步的引擎机制与插值 |
| 客户端权威 trade-off | 为什么先客户端权威：作弊面 vs 开发成本 vs 手感 |
| （stretch）预测回滚 | 误判补偿、reconciliation——答上来就是加分项 |
| GAS 衔接 | 下一项目的 GameplayEffect 属性复制直接复用本项目心智模型 |

## 八、与丧尸线的关系

- `zombie-horde` 分支永久冻结：P1 完整闭环（刷怪→攻墙→胜负）+ P2a 槽位堆系统（334 槽位网格+攀爬 FSM）
- 面试叙事："做过单机 AI 波次攻防（行为链/状态机/群体槽位算法，见博客），正在做联机 PvP（复制/RPC/权威模型）"——一单机一联机，故事完整
- 未来若做"联机坦克守城 PvE"，丧尸线在分支上可直接复活接入

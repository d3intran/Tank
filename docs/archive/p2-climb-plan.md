# P2 演进规划：爬墙优先

> 2026-09-09。P1 行为闭环当日全部验证通过，本文件是下一阶段（P2a 爬墙）的设计稿 + P1 收尾清单。

## 战报：P1 行为闭环已全部实测验证（2026-09-09）

- 刷怪 20 只 @1s ✓ / 冲墙寻的 ✓ / 碾压死 ✓ / 炮击直杀+溅射 ✓ / 击飞（×5 力度）✓ / 打残（放倒+半速）✓
- 胜利"守住"×2 ✓ / 失败"城墙陷落"×2 + 自动重开 ✓（日志实锤）
- 工具链：Scripts/editor.deno.ts（status/open/close/execfile）+ build.deno.ts + PIE 远程启动——编辑器开关、构建、试驾全流程 Agent 接管 ✓

## P1 剩余两项（小）

1. **100 胶囊同屏 fps 验证**：SmokeSpawner 的 TotalToSpawn 调到 100 跑一局，肉眼+帧率验证（预计轻松过，量级远低于 800 预算）
2. **一轮 3~5 分钟节奏**：现在一轮约 60s，太快。调参方向：城墙 HP↑ / 刷怪间隔↑ / 攻击 DPS↓ / 多刷怪器错峰。试驾会调参项，需要真人玩几轮定手感

## 演进顺序：先爬墙（P2a），后 Mass（P2b）

对照 ROADMAP 风险清单的理由：

- **爬墙 = 核心威胁链路**，行为不成立则概念不成立——最高风险先验证
- 爬墙是**玩法设计问题**（要反复试驾调手感），Mass 是**工程问题**（City Sample 有参考可抄）
- 状态机设计（WalkToWall→Climbing→DespawnAtTop）与 Actor 架构解耦，迁 Mass 时整体平移
- 迭代铁律"一轮只动一个变量"：爬墙动行为，Mass 动架构，不混在一轮

## P2a 爬墙灰盒设计（按 2026-09-08 拍板方案落地）

### 1. 隐形斜坡

- 几何：45° 斜面贴墙根，宽=墙长，水平深=墙高/tan45°=774cm
- **正式命名碰撞通道 `ClimbRamp`**（写进 DefaultEngine.ini，不再用未命名 GameTraceChannel 的裸枚举做法）
- 通道规则：仅丧尸查询/阻挡；坦克、炮弹、瞄准视线**显式** Ignore——吸取直击 bug 教训：配对规则是"最宽容者生效"，双方矩阵都要显式配置并写进代码注释

### 2. 丧尸状态机扩展

```
WalkToWall ─（进坡脚范围）→ Climbing ─（t=1 到顶）→ DespawnAtTop（消失+扣墙血）
```

- Climbing = **参数化攀爬**（纯运动学，不走扫掠）：位置 = 坡底 + t×坡向量 + 横向随机偏移
- t 前进速度 = ClimbSpeed/坡长（可调），攀爬中朝向坡面
- 坡上被击杀 → 切坠落态（已有击飞逻辑直接复用）
- 打残在攀爬中生效：半速爬

### 3. 伤害模型转变（⭐需要拍板的设计点）

P1 现状是"啃墙根持续 DPS"（占位逻辑）。拍板闭环是"登顶瞬间消失+扣血"。二选一：

- **方案 A（推荐）**：删除啃咬 DPS，唯一伤害来源=登顶一次性扣（如 50/只）。墙根堆叠的丧尸不扣血，只是排队等爬——逼玩家打坡体
- 方案 B：保留墙根啃咬 DPS + 登顶扣血双威胁——压力更大，"坦克清墙根"更有意义，但偏离已拍板的单威胁闭环

建议 A 先试驾，压力不够再叠 B。

### 4. 堆山视觉（流，不是结构）

- 坡上丧尸按 t + 横向偏移自然错开，后方源源补位 = 流
- 灰盒阶段不做摆姿尸体装饰层（P2c 再加 ISM 假尸增厚视觉）

### 5. 坦克与火炮交互

- 坦克撞坡底：坡脚丧尸照常 CrushRadius 判死；攀爬者在盒高之上够不着（拍板：坦克清底、炮清高处）
- 炮打坡面：命中点径向伤害，攀爬者按当前位置参与距离判定——半空击杀切坠落态，给"打塌一段"的观感

## P2b Mass 迁移（爬墙行为锁定后）

- 胶囊 Pawn → MassEntity + processor 链（寻的/攀爬/坠落各一个 processor）
- 渲染：MassUpdateISMProcessor 批渲染（数百只一个 draw call）
- 迁移完成 = 800 只 @60fps 验收（ROADMAP P2 DoD）
- 参考：Epic Mass 入门 60 分钟 + City Sample + `docs/zombie-tech-route.md`

## 检查点

- **P2a 验收**：20 只持续爬坡登顶扣血、不干预 60s 内必输；坦克清底+炮清高处能守住
- 攀爬行为试驾满意后再迁 Mass（行为锁定，迁移一次到位）

---

# 实施状态（2026-09-09 暂停，待调查）

## 已落地（代码在 Source/Tank/，全部编译通过）

- **高度场模型 v4**：`UClimbManager` 锥形高度场（峰心=代理墙 FaceCenter，PileRadius 774，踩倒 3 具抬升 60cm，堆顶齐墙=成型）；`AZombie` 状态机 WalkToWall → AtPile（运动学贴场挤压，Z=场高，踩踏概率 0.5/s）→ Trampled（躺平永久）→ Dead（击飞坠落）。堆区内纯运动学推进（绕开拱门/门洞碰撞死角）
- **已验证工作**：冲门寻的 ✓、进堆区切 AtPile ✓、被踩倒躺平 ✓（日志确认）、登顶扣血 ✓、堆顶随踩倒抬升 ✓
- **P1 全部反馈已验证**：直杀/溅射/击飞（2800）/打残（放倒半速）/碾压（2200 径向抛）/胜负面板/自动重开
- **真实城墙接线**：隐形代理 ADefWall（SmokeWall_Proxy）贴城门，丧尸打的就是城门位置；GameMode 通用组件发现；灰盒墙已删
- 工具链：Scripts/editor.deno.ts（status/open/close/exec/execfile，base64 桥）、smoke_*.py 系列、PIE 远程启停

## 未解决（调查再开时的入口）

**症状**：丧尸挤成一坨卡在拱门凹龛处（非中央门心），锥面高度场没有视觉展开，无"踩坡"观感。

**疑点清单（按嫌疑排序）**：

1. **缺水平分离力（最大嫌疑）**：全丧尸目标同一点（PileCenter），水平面无避让/排斥——100 只同路径汇聚必然叠成一坨。WWZ 类的"流"视觉核心是 separation force。修法：AtPile/WalkToWall 加 O(n²) 推斥（20-100 只量级够用）或空间网格
2. **拱门凹龛的碰撞捕获**：WalkToWall 是扫掠（挡 WorldStatic），城门凹龛+门洞封堵碰撞会提前卡住丧尸；锥区切换条件 `Dist2D < PileRadius+120`（894cm）——卡在横向偏移大的龛里时到门心距离可能超阈值 → 永远不切换、永远挤在龛里
3. **PileCenter 对齐**：FaceCenter=bbox 几何中点，未必是"正中央大门"（5 门分布）；需 PIE 实测门中心 X 与 PileCenter.X 对比
4. 旧值残留嫌疑：代理 ClimbManager 的参数曾被多轮脚本改写，PIE 前建议探针核对实际生效值（probe_params.py 模式）

**调查工具**：Scripts/probe/probe_pie.py（PIE 内 Actor/状态探针，注意 get_editor_world 在 PIE 期间返回 None、get_game_world 在 PIE 结束后返回残留世界）、Scripts/probe/probe_params.py、LogsToolset（有陈旧缓冲，不能只信日志，截图/探针交叉验证）

## 本轮教训（已入项目记忆 p2a-climb-pipeline.md）

- `set_world_scale3d` 是绝对世界缩放（相对 100cm 立方），不是"目标/默认"比例
- 隐藏 Actor（SetActorHiddenInGame）包围盒可能退化 → 依赖 bbox 的几何全崩；隐藏用"Actor 可见+网格组件不可见"
- UE 碰撞配对=最宽容者生效（任一方 Ignore 即互不作用），自定义通道双方都要显式配置
- 5.8 Python：StaticMeshActor 无 add_component_by_class、组件无 register_component → 动态挂组件用"隐形代理 Actor"
- PIE 期间 get_editor_world()=None；PIE 后 get_game_world()=残留世界


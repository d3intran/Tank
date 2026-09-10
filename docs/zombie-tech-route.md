# 丧尸尸潮技术路线规划

> 2026-09-09 调研定稿。回答一个问题：尸潮本体用 Niagara 还是别的？
> 依据：Stray Spark Studio《Building Crowds and Traffic in UE5: Mass AI and 10,000 NPCs at 60fps》（2026-03，含三档硬件实测）、Epic Mass 入门教程、City Sample 架构。

## 结论

**丧尸本体不用 Niagara。Niagara 只管 VFX。**

丧尸群体 = **Mass Entity（逻辑）+ ISM 批渲染（表现）+ VAT（动画）+ Niagara（死亡特效）**。

## 为什么 Niagara 不适合做丧尸本体（四个技术不匹配）

### 1. 命中回读难

炮弹径向伤害需要"距落点优先选受害攀爬者"，坦克碾压需要逐体判死——这些都是**游戏侧主动查询实体位置/状态**。Niagara 粒子数据在 GPU 侧模拟，游戏逻辑要回读只能：
- 走 Data Interface 在 Niagara 脚本里绕一圈再导出
- 或自己维护一张 CPU 缓存表与 Niagara 同步

两条路都别扭，且每个玩法系统（炮弹、碾压、城墙计分）都要重复这个 hack。Mass 的 Fragment 本来就在 CPU 连续内存里，`EntityQuery` 直接查，这是它天生的主场。

### 2. 状态机与玩法联动弱

alive / climbing / falling / dead 四态切换，死亡要联动城墙扣血、波次统计、击杀反馈。Mass + StateTree 就是为这种"个体状态 + 群体调度"设计的一等公民；在 Niagara 脚本里做状态迁移要靠 curve/bool 信号传值，是黑科技不是工程。

### 3. 量级错配

Niagara 的优势场景是**万级以上 GPU 模拟**。你的预算是 800 @ 60fps，Stray Spark 实测：

| Agent 数 | RTX 4080 | RTX 4060 |
|---|---|---|
| 1,000 | 1.2ms | 1.8ms |
| 5,000 | 2.8ms | 4.5ms |
| 10,000 | 4.6ms | 7.8ms |

800 只在 Mass 路线下成本不到 2ms，极其宽裕。用 Niagara 等于为了 800 只的需求引入一套 GPU 回读架构债。

### 4. 假堆爬坡是运动学，不是模拟

隐形斜坡 + 参数化攀爬在 Mass processor 里就是"读坡面参数 → 改 Transform"，20 行代码。Niagara 里做精确沿坡面运动 + 与坦克碰撞盒交互反而无从下手。

### Niagara 的正确位置

- 命中/死亡反馈：血雾、爆浆粒子（P1 验收就要求）
- 炮口焰、炮弹尾迹、爆炸、履带扬尘（P3）
- 碎尸飞溅的粒子层配合 Chaos 碎块（P2）
- **可选远景装饰尸潮**（城外黑压压一层、不参与玩法、纯 GPU 粒子 + billboard）——进 backlog，P4 之后想要再加，与玩法层完全解耦

## 与 City Sample 的关键差异（别照抄）

City Sample 人群 = ZoneGraph 沿路径行走（人行道/车道）。你的丧尸是**径向冲墙**——从四面八方向墙根汇聚，没有"路径"概念。

- **不需要 ZoneGraph**：移动 = 向量指向墙根 + MassAvoidance 避让，够了
- **需要自定义 Processor**：寻的（朝墙根）、攀爬（沿坡面）、登顶（despawn + 扣血）都不在 MassAI 现成组件里，自己写 processor——这正是 Mass 架构的预期用法
- City Sample 该抄的是：Representation/LOD 管理、ISM 批渲染、EntityConfig 资产组织方式

## 完整分层方案（按阶段）

### P1（现在）：普通 Pawn + 胶囊

- 20~100 只 AActor 占位丧尸，自绘扫掠移动（与坦克同构，为 Mass 迁移铺路）
- 验证行为链：刷新 → 直线冲墙 → 登顶 despawn → 城墙扣血 → 输赢
- 碾压 = overlap 判死 + Niagara 爆浆粒子（最简命中反馈，P1 验收项）
- 此阶段不上 Mass——规模太小，先验证行为，迁移成本留到 P2

### P2a：迁 Mass + ISM

- `MassEntityConfig`：MovementTrait + 自定义 `ZombieStateFragment`（枚举：Alive/Climbing/Falling/Dead）
- 处理器链：径向寻的 → 避让 → 状态迁移 → ISM 批渲染
- 渲染：`MassUpdateISMProcessor` 把 Transform 批量喂给 ISM，800 只 ≈ 1 个 draw call
- 命中检测：炮弹爆炸时径向遍历实体位置（800 个坐标直接算距离都够快，真不够再上空间哈希）
- 碾压：坦克碰撞盒每帧查询附近实体，判死 + Niagara 血雾 + Chaos 碎块（并发 ≤30，2s 回收）
- 死亡：销毁实体 + 特效；半空击杀切 Falling 态（纯运动学重力，1s 落地消失）

### P2b：假堆爬坡

- 隐形斜坡（`ClimbRamp` 独立碰撞通道，仅丧尸攀爬查询生效；坦克/炮弹/视线全部忽略）
- 攀爬处理器：墙根拥挤检测 → 沿坡面参数化上升（坡面 UV 参数 → 世界坐标）
- 登顶触发盒：despawn + 城墙 HP 扣血
- ISM 摆姿尸体装饰层：静态假尸铺坡面增加视觉厚度，不参与逻辑
- 落地顺序先写通道隔离自验：坦克开过隐形坡不卡、炮弹穿过隐形坡命中后方丧尸

### P3：换皮（VAT 路线）

- **AnimToTexture 插件**（引擎自带）：把 Quaternius/Mixamo 骨骼动画（walk/climb/death 各一条）烘成位置纹理 + 法线纹理
- ISM 材质用 World Position Offset 采样纹理还原动画；**per-instance custom data 存 anim index + 随机相位偏移**——每只动画错开，杜绝"复制粘贴人海"
- 近处 LOD：Mass Representation Trait 配高保真模板 Actor——相机 25~30m 内换真骨骼（≤30 只），其余全 ISM VAT
- LOD 过渡：dither 抖动透明 + 每只随机距离偏移（防成排同时切换的"波纹"）
- **成本探针**：先烘一只测单只成本 → 决定 800 还是降 500

### 分层 LOD 参照（Stray Spark 四层模型）

| 层 | 距离 | 表现 | 单只成本 |
|---|---|---|---|
| Tier 1 | 0~25m | 真骨骼 Actor（≤30 只） | 0.05~0.15ms |
| Tier 2 | 25~60m | VAT 静态网格 | ~0.002ms |
| Tier 3 | 60~150m | ISM 平材质 | 更低 |
| Tier 4 | 150m+ | billboard / 剔除 | ≈0 |

你的场地不大，实际 Tier 1/2 大概率就够。骨骼动画 vs VAT 差 **50 倍**成本，所以"近处骨骼数量"是比总量更关键的阀门。

## 常见坑对照表

| 坑 | 对策 |
|---|---|
| 骨骼 Actor 数失控 | Tier 1 距离压到 25~30m，不心软 |
| 动画相位全同步 | per-instance 随机相位第一天就做 |
| 速度整齐划一 | ±20% 随机速度 |
| 只在视锥内模拟 | 转身尸群瞬移；在相机周围半径内持续模拟，视锥只做渲染剔除 |
| 死亡表现失控 | Chaos 碎块并发 ≤30、存活 2s、池化复用 |

## 换皮防翻车：三份契约 + 一次彩排 + 保底开关

换皮出问题的本质不是美术，是**逻辑层与表现层的接口没有提前固化**。P3 翻车的四种典型：pivot/比例错位（脚悬空、半截入土）、动画状态缺覆盖（爬墙滑步）、per-instance 数据对不上（全员同步鬼畜）、成本爆炸（VAT 材质超预算）。四条对策：

### 契约一：空间契约

- 逻辑胶囊永久固定：脚底原点、总高 180cm、半径 40cm（数值 P2 冻结后不再动）
- 任何皮必须归一化到该框架：pivot 在两脚之间地面、模型脚踩原点、头顶 180cm
- 进资产管线时一次性修正（吸取长城管线教训：FBX 单位/轴向/pivot 三件套必须先核）
- 硬规则：**逻辑层只读胶囊，永不读网格**——皮不影响逻辑，逻辑不知道皮存在

### 契约二：状态契约（状态即接口）

状态枚举 P2 冻结：`Alive / Climbing / Falling / Dead`。每个状态映射一条动画，列表如下，皮的第一验收 = 四态全覆盖：

| 状态 | 动画 | 备注 |
|---|---|---|
| Alive（平地） | walk | 最先要有 |
| Climbing | climb | P2b 前可先用 walk 占位 |
| Falling | fall/stumble | 可复用 death 前半段 |
| Dead | death | 一条倒地即可 |

### 契约三：数据契约（ISM 槽位布局）

- per-instance custom data 布局 P2 定死：slot0=anim index、slot1=相位偏移、slot2=变体/色相…
- **P2 胶囊阶段就按此布局写代码**（胶囊哪怕只用 slot1 做节奏错开也要走真槽位）
- P3 材质读同一布局——换皮 = 换网格和材质，槽位协议不变

### 一次彩排：P2 中段"假换皮"

- 拿任一人形静态网格（引擎自带 Mannequin 摆个跑步姿势就行）替换胶囊 ISM 的网格
- 跑通全流程：刷新 → 冲墙 → 堆爬 → 碾压 → 登顶扣血
- 一次性暴露：pivot 错位、脚悬空、比例错误、LOD 切换爆闪、custom data 读取错误
- 成本≈0（零烘焙零动画），收益 = 把约 80% 的换皮风险提前一个阶段引爆

### 保底开关：胶囊永远可回退

- 换皮走配置（MassEntityConfig 的 StaticMeshInstanceDesc / Representation），**不删胶囊路径**
- 皮出问题时一键切回胶囊 → 二分定位"是皮的锅还是逻辑的锅"
- 调试期可同屏混渲：逻辑用胶囊、视觉用皮，错位一眼看出

### AnimToTexture 冒烟探针（扩展版成本探针）

P2 结束前：一只 Quaternius 丧尸 + walk 动画烘焙 → 进 ISM → 游戏内截图。一次验证三件事：

1. 单只成本——800 预算还成立吗
2. WPO 与当前渲染管线兼容性——VAT 网格保持非 Nanite 低模路线，避开兼容坑
3. 烘焙管线全链路有没有断点

探针通过后，P3 才是"重复已经做过的事"，而不是开盲盒。

### 选皮准入清单（P3 买模型/下模型时逐条过）

| 准入项 | 要求 | 说明 |
|---|---|---|
| 骨架+动画 | 必须带骨骼动画（walk/death 至少两条），或可重定向到 Mixamo 标准骨架 | VAT 烘的是骨骼动画，静态模型对路线无用；**动画覆盖是最稀缺属性，候选名单提前扫** |
| 面数 | ≤1 万三角面，最好 3~5k | 5k 面 × 800 只 ≈ 400 万三角形 |
| Pivot/比例 | 可归一化到胶囊框架（脚底原点、180cm 高） | 契约一 |
| 版权 | CC0 / CC-BY（CC-BY 署名） | 游戏 rip 素材绝对不可用（ROADMAP 已定） |

免费可满足：Quaternius（CC0，角色包自带骨架+动画库）、Mixamo（免费丧尸类角色+动画，重定向导出 FBX）。"随便找" = 在带动画的人形低模里随便挑顺眼的。

后续加变体（P4 快速/重甲）= 多烘几套 VAT + anim index 走 slot0，架构已预留。

## 检查点（写进迭代计划）

1. **P2 结束前**：一只 Quaternius 丧尸 + walk 动画烘一次 VAT，实测单只成本 → 敲定 800 or 500
2. **P2b 落地时**：ClimbRamp 通道隔离自验（坦克不卡、炮弹穿透、丧尸可爬）三件套截图留档
3. **P3 复测**：换皮后 800 @ 60fps 重跑（ROADMAP 已有此项）

## 参考来源

- Stray Spark Studio: Building Crowds and Traffic in UE5 (2026-03) — 实测数据与四层 LOD
- Epic 官方：Mass 入门 60 分钟（dev.epicgames.com）
- Epic 官方：City Sample（MassCrowd 生产级参考）
- ROADMAP.md P2/P3 既有拍板（本文件不改变任何已拍板结论，只落技术细节）

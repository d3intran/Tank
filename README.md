# Tank — UE5 多人在线坦克 FFA

UE 5.8 纯 C++ 项目（模块 `Tank`），灰盒多人坦克对战：**1 台主机 + 2 个客户端**（局域网 Listen Server），
先到 **5 杀**者赢，回合自动重开。坦克模型来自 ztz-88a，地图为自定义灰盒竞技场。

> 早期版本是「驾驶坦克守城打丧尸」，该玩法已在 M0 移除；相关设计文档见
> [`docs/archive/`](docs/archive/)，代码保留在 `zombie-horde` 分支（不参与构建）。

## 当前状态

见 **[`docs/STATUS.md`](docs/STATUS.md)**（里程碑进度、已验证项、已知问题、下一步）。
开发计划书与逐轮排查记录见 [`docs/tank-ffa-plan.md`](docs/tank-ffa-plan.md)。

## 目录

```
Source/Tank/         C++ 源码（Pawn / PlayerController / GameMode / GameState / PlayerState / HUD / 弹药 / 血量）
Content/             关卡 Lv1-TankArena.umap、坦克与城市资产、Enhanced Input 资产（tank/inputs）
Config/              DefaultEngine.ini / DefaultGame.ini（含 FFA 规则参数 KillsToWin / RoundRestartDelay）
Scripts/             自动化脚本（见下）
Content/Python/      编辑器内 Python：init_unreal.py（文件桥）、tank_shot.py（截图辅助，防 GC 崩）
docs/                计划书 + 当前状态 + 归档
.workbuddy/memory/   项目长期笔记与每日工作日志（给下一个接手的人/AI 看）
Binaries, Intermediate, Saved, DerivedDataCache, Bridge/   构建与运行时产物（已 gitignore）
```

## 环境

| 项 | 值 |
|---|---|
| 引擎 | `E:\UE_5.8`（UE 5.8） |
| 工具链 | MSVC 14.51 （VS 18 Community），`deno` 2.x |
| 编辑器 MCP | 项目自带 `ModelContextProtocol` 插件，`http://127.0.0.1:8000/mcp`（见 `.mcp.json`） |

## 构建 / 启停 / 验证

**所有脚本都要在沙箱外跑**（编辑器与 UBT 需要写 `AppData`、`ProgramData`）。

```bash
# 构建（UBT 优先，失败自动降级「按 rsp 直调 MSVC」）
deno run -A Scripts/build.deno.ts Development

# 编辑器启停（close 会先存脏包再 QUIT_EDITOR 优雅退出）
deno run -A Scripts/editor.deno.ts status
deno run -A Scripts/editor.deno.ts open
deno run -A Scripts/editor.deno.ts close

# 在正在运行的编辑器里执行 Python（零转义损耗，推荐 execfile）
deno run -A Scripts/editor.deno.ts execfile Scripts/pie/pie_turret_report.py --timeout=60
```

改代码 → `close` → `build` → `open`。**编辑器开着时 DLL 被锁，链接会失败**；
`UPROPERTY(Config)`（如 `KillsToWin`）也只在引擎启动时读一次。

### 测试方式（PIE 三开）

PIE 设置沿用已保存配置：`PlayNetMode=PIE_ListenServer` / `RunUnderOneProcess=True` /
`PlayNumberOfClients=3` → **3 个世界 = 1 server + 2 client**。

- `Scripts/pie/pie_start.py` / `pie_stop.py` 起停，`pie_status.py` 看存活与车辆归属
- 状态巡检：`pie_drive_report.py`（位姿）、`pie_turret_report.py`（炮塔/复制）、`pie_rules_report.py`（规则/比分）
- 驱动坦克：Python 拿不到 EnhancedInput 子系统，**只能按窗口投键** →
  `Scripts/tools/win_list.py` 找窗口，`Scripts/tools/win_key.py "Client 1" W 1.5 --activate`

脚本清单与用法见 [`Scripts/README.md`](Scripts/README.md)。

## 已知约束（踩过的坑，别重踩）

- 客户端世界里的坦克是**网络生成**的，`Tick` 注册与输入绑定会整个丢失 →
  `ATankPawn::EnsureClientReady()` 在 `PostNetReceive` 上做幂等自愈（Tick 段覆盖所有车，输入段只管本机车）。
- `unreal.EditorAssetLibrary` 在本工程失效（连 `/Game` 都报不存在），取资产用 `unreal.load_asset`。
- UBT 的 link 阶段不稳定（`LNK1201` 写 PDB 失败），且**失败会把 `.lib`/`.pdb` 截断成 0 字节**并毒化后续构建 ——
  `build.deno.ts` 每次开跑前会清掉 0 字节产物。
- 从桥里调 `take_high_res_screenshot` 有概率让编辑器崩（latent action 代理被 GC，回调悬空）→
  统一用 `Content/Python/tank_shot.py`，并预期可能崩溃。

更完整的环境与日志解读约定见 [`.workbuddy/memory/MEMORY.md`](.workbuddy/memory/MEMORY.md)。

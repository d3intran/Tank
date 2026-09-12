# 项目长期笔记 — E:\UE\Tank（Tank FFA / UE 5.8）

## 自动化脚本（先看这里，别手工重复造）
`Scripts/` 下已有成套接管脚本，**都用 `deno run -A` 跑，且必须在沙箱外**
（编辑器/UBT 要写 `AppData`、`ProgramData`，沙箱里会被拦掉）。完整索引见 `Scripts/README.md`：

```
Scripts/
├─ build.deno.ts          构建入口
├─ editor.deno.ts         编辑器启停 + 文件桥执行 Python（入口）
├─ level/                 关卡编辑（level_*.py、m0_place_spawns.py）
├─ pie/                   PIE 探针与验证（pie_*.py、verify_m2_fix.py、wheel_*.py）
├─ probe/                 一次性 API 探针
└─ tools/                 win_list.py / win_key.py / ue_bridge.py / split_tank_mesh.py
```

| 常用 | 用途 |
|---|---|
| `Scripts/editor.deno.ts <status\|open\|close\|exec\|execfile>` | 编辑器启停 + 桥执行 Python。`execfile Scripts/xxx.py` 是零转义损耗的推荐姿势。`--force` 强杀，`--no-save` 不存包 |
| `Scripts/build.deno.ts [Development\|DebugGame]` | UBT 优先，失败自动降级手动 rsp 编译链接；编辑器开着会直接退出（DLL 被锁） |
| `Scripts/tools/win_list.py [关键字]` | ctypes 列可见顶层窗口（含 hwnd/pid/标题），定位 PIE 客户端窗口 |
| `Scripts/tools/win_key.py "<窗口标题子串>" <键> [秒] [--activate]` | 向指定窗口 PostMessage 投键（不抢焦点），用于驱动 PIE 客户端 |
| `Scripts/pie/pie_turret_report.py` / `pie_drive_report.py` / `pie_rules_report.py` | PIE 状态巡检：炮塔复制 / 位姿 / 规则比分 |

**编辑器启动必须走 WMI**：`editor.deno.ts open` 里已改成
`Invoke-CimMethod Win32_Process Create`。直接 `Deno.Command spawn` 的子进程会挂在调用方的
Windows Job Object 上，CLI 一退出编辑器就被连带回收（症状：MCP 8000 已就绪、40s 后进程消失，
`Tank.log` 停在模块加载中途且**没有任何 shutdown 记录** —— 不是崩溃）。PowerShell 的
`Add-Type`（P/Invoke）在本机被安全策略禁止，所以窗口操作用 Python ctypes 而不是 PowerShell。

## 构建（踩坑后固化，务必按这个来）
`Scripts/build.deno.ts` 是唯一入口（沙箱外跑）。它的行为：

1. 先跑 UBT。UBT 在**本机必定在 link 阶段失败**：`LNK1201` 写
   `Binaries/Win64/UnrealEditor-Tank.pdb` 失败，并**把 .lib/.pdb 截断成 0 字节、删掉旧 DLL**。
   但 UBT 的 **UHT 与编译阶段是成功的** —— 改 `UFUNCTION`/`UPROPERTY` 后仍需借它重生成代码
   （直调 cl 会报 `C2511 没有找到重载的成员函数`）。
2. 失败后自动走兜底：清 0 字节产物 → 按 rsp 逐个 cl → `link /LIB` → `link dll`（**顺序不能反**，
   DLL 链接会按 `/IMPLIB` 覆写导入库）。

兜底路径手工等价命令（**工作目录必须是 `E:\UE_5.8\Engine\Source`**，`Tank.Shared.rsp` 里全是相对 `/I`）：
```bash
cd /e/UE_5.8/Engine/Source
CL="/c/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe"
LD="/c/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/link.exe"
D="E:/UE/Tank/Intermediate/Build/Win64/x64/UnrealEditor/Development/Tank"
"$CL" @"$D/TankPawn.cpp.obj.rsp"        # 每个改动的 TU 都要重编
"$LD" /LIB @"$D/UnrealEditor-Tank.lib.rsp"
"$LD" @"$D/UnrealEditor-Tank.dll.rsp"   # 产出 Binaries/Win64/UnrealEditor-Tank.dll
```

- 改公共头（如 `TankPawn.h`）后，**所有 include 它的 TU 必须一起重编**，否则 vtable/ABI 不一致。
  直接重编 `BattleGameMode / BattleHUD / TankPawn / Module.Tank.gen` 这四个最省事。
- **「编译失败但没有任何诊断」这个老结论是错的**：MSVC 的诊断走 stderr，而脚本的 `sh()`
  只读了 stdout 就把它丢了；现已同时收 stdout+stderr 并优先打 error 行。
- 中间目录里残留过**源文件已删除的陈旧 rsp**（`ClimbManager/DefGameMode/DefHUD/DefWall/
  Zombie/ZombieSpawner`，来自本项目更早的形态）。它们会让兜底编译以 `C1083 无法打开源文件`
  中断整轮；脚本现在会识别并跳过，手工排查时直接删掉对应的 `.obj.rsp`/`.obj`。
- `Intermediate/.../Tank/*.obj.rsp` 的**第一行就是该 TU 的 .cpp 绝对路径**，判断 obj 是否过期
  要拿它跟源文件比时间（rsp 自身不会因源码改动而更新，头文件更不在其内）。

### 编辑器开着时无法重链 DLL
`UnrealEditor.exe` 常驻会锁住 `Binaries/Win64/UnrealEditor-Tank.dll`（`LNK1104`）。
`UPROPERTY(Config)` 的值（如 `KillsToWin`）也只在引擎启动时读一次 —— 所以改代码/改 ini 后
按 `close → build → open` 走一遍。`close` 走文件桥「存脏包 → QUIT_EDITOR」，能优雅退出。

## 编辑器内调试通道
- 文件桥：`Content/Python/init_unreal.py` 轮询 `Bridge/queue/*.json`，结果写 `Bridge/results/<uuid>.json`。
  编辑器必须在运行。`editor.deno.ts execfile` 已封装（base64 传代码，规避 bash→deno 的引号损耗）。
- `Scripts/pie/pie_*.py` 是 PIE 探针脚本（`out.append(...)` 约定，**禁止对 out 重新赋值**），跑前需 PIE 已在运行。
- 关卡脚本（`Scripts/level/level_*.py`）执行后要显式保存，PIE 运行期间 `save_current_level()` 会失败，先停 PIE。

### Python API 的坑（都实证过，按踩坑顺序）
- **`unreal.Rotator` 按位置传参的顺序是 (roll, pitch, yaw)，不是 (pitch, yaw, roll)！**
  `Rotator(0, 90, 0)` 实际是 pitch=90（车头朝天）。**一律用关键字**：
  `unreal.Rotator(pitch=..., yaw=..., roll=...)`。
  踩过的现场：坡道朝向立起来、出生点朝向全错（8/8 偏，想要的 yaw 进了 pitch）、瞬移后坦克 nose-up。
  已修：`Scripts/probe/probe_rotator_order.py` 是判别探针；`level_spawn_facing_fix.py` 一次性修好了 8 个出生点。
- **场上真正可行驶面是 road_hd / road_hd2（顶面 Z≈2.1）**，Floor（顶面 Z=-40）只是它下面的基底。
  任何「贴地/坡道/出生高度」都要以 2.1 为基准，按 -40 算会露出几十厘米的坎。
  取地面高度别猜，向下 trace 一发最稳（见 `level_cover_ramps.py` 的 ground_z_at）。
- `SystemLibrary.delay()` 是 latent，桥脚本里不能用（会阻塞游戏线程）；分两次桥调用即可。
- **`unreal.EditorAssetLibrary` 在本工程整个是坏的**：`load_asset` / `does_asset_exist` /
  `list_assets` 一律返回空（连 `/Game` 都报不存在），但同一次执行里 `unreal.load_asset("/Game/...")`
  正常、`AssetRegistryHelpers.get_assets_by_path` 也能列全。**取资产一律用 `unreal.load_asset`**。
- **Python 拿不到 EnhancedInput 的 LocalPlayer 子系统，无法注入输入**：`ULocalPlayer::GetSubsystem`
  是模板（未反射）；`USubsystemBlueprintLibrary` 因 `BlueprintInternalUseOnly` 未进 Python 绑定
  （`unreal.SubsystemBlueprintLibrary` 不存在）；`GameInstance` 无任何 player 成员；
  `PlayerController::GetLocalPlayer`/`Player` 也未反射。三条探针交叉确认。
  → 要驱动 PIE 里的坦克只能用 `Scripts/tools/win_key.py`（按窗口投键）。
- 炮塔朝向可从 Python 直接读：`pawn.current_turret_yaw` / `pawn.net_turret_yaw` /
  `pawn.net_gun_pitch`（炮塔/火炮组件是 `turret_pivot` / `gun_pivot`，但 SceneComponent 没有
  `get_relative_rotation`，读角要用前者）。

## 日志解读约定（血泪教训）
- **每个 PIE 世界各自给 Actor 命名**：日志里会出现多个 `TankPawn_0`，是不同世界里的不同车，
  别当成同一辆。跨世界对齐只能靠 `t=`/Frame/事件序列/位置。
- **日志里没有任何世界前缀**。判断一辆车在哪个世界，看它那条日志的 `Role=`：
  3=Authority(服务器) / 2=AutonomousProxy(本端客户端) / 1=SimulatedProxy(远端)。
- 拿捏不了世界归属时，用 `Scripts/pie/pie_turret_report.py` 这类脚本直接按世界枚举（服务端 =
  PC 最多的那个世界；**三个 PIE 世界同名，只能用对象身份比较，不能按名字聚合**）。

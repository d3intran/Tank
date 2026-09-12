# Scripts 索引

所有脚本都在项目根目录下执行，**且必须在沙箱外运行**（编辑器与 UBT 需要写 `AppData` / `ProgramData`）。

```
Scripts/
├─ build.deno.ts          构建入口
├─ editor.deno.ts         编辑器启停 + 文件桥执行 Python（入口）
├─ level/                 关卡编辑（改完要显式保存；PIE 运行中保存会失败）
├─ pie/                   PIE 内的探针与验证脚本（需要 PIE 已在运行）
├─ probe/                 一次性 API 探针（查「这台机器上哪些接口真的可用」）
└─ tools/                 窗口/桥/网格等辅助工具
```

## 入口两个

```bash
deno run -A Scripts/build.deno.ts [Development|DebugGame]   # 编辑器开着会直接退出（DLL 被锁）
deno run -A Scripts/editor.deno.ts status|open|close|exec|execfile
```

`editor.deno.ts`：
- `open` 经 WMI 创建进程（`Win32_Process.Create`）。**不能用直接 spawn** —— 子进程会挂在调用方
  Job Object 上，脚本一退出编辑器就被连带回收（症状：MCP 8000 已就绪、40s 后进程消失、
  日志停在模块加载中途且无 shutdown 记录）。
- `close` 走文件桥「存脏包 → QUIT_EDITOR」，优雅退出；`--force` 才强杀。
- `execfile Scripts/xxx.py` 是执行桥脚本的推荐姿势（base64 传代码，零引号转义损耗）。

## pie/（PIE 探针，`out.append(...)` 约定，**禁止对 out 重新赋值**）

| 脚本 | 用途 |
|---|---|
| `pie_start.py` / `pie_stop.py` / `pie_status.py` | 起停 PIE / 看存活与车辆归属 |
| `pie_drive_report.py` | 按世界枚举每辆车：归属、位姿、`Tick 开关`、炮塔角 —— **移动类问题首选** |
| `pie_turret_report.py` | 同上看 `current_turret_yaw` / `net_turret_yaw` / `net_gun_pitch` —— **炮塔/复制类问题首选** |
| `pie_rules_report.py` | 读 GameMode/GameState 的 `kills_to_win` 与各 PlayerState 比分 —— 验证 Config 是否生效 |
| `pie_inspect.py` / `pie_m3_verify.py` | 状态巡检（占有/重生/保护） |
| `pie_kill*.py` / `pie_m3_kill.py` | 制造击杀。**注意 `pie_kill.py` 用受害者自己的 Controller 当 Instigator = 自杀，不计分**；验证计分要用 `pie_m3_kill.py` |
| `pie_screenshot.py` / `pie_*_shot.py` | 编辑器内截图（统一走 `Content/Python/tank_shot.py`，见下） |
| `pie_force_victory.py` | 强制造出胜利态 |
| `pie_startspot_probe.py` / `pie_bug2_probe.py` | 出生点选点判别实验 |
| `verify_m2_fix.py` | M2 占有链路端到端验证 |
| `wheel_*.py` | 负重轮相关的探针与驱动 |

## level/（关卡编辑）

`level_recon.py`（侦察尺寸）→ `level_ffa_setup.py`（出生环 + 掩体）→ `level_ffa_verify.py`（复核）
→ `level_cover_fix.py`（改掩体）→ `level_edge_step.py`（三面边界台阶）→ `level_bounds.py`（读边界）。
`m0_place_spawns.py` 是最早的出生点摆放脚本。

关卡改动只有 `Content/Lv1-TankArena.umap` 一个文件，`git checkout -- <该文件>` 即可回滚。

## tools/

| 脚本 | 用途 |
|---|---|
| `win_list.py [关键字]` | ctypes 列可见顶层窗口（hwnd/pid/标题），定位 PIE 客户端窗口。用 ctypes 是因为本机 PowerShell 的 `Add-Type` 被安全策略禁止 |
| `win_key.py "<标题子串>" <键> [秒] [--activate\|--foreground]` | 向指定窗口 PostMessage 投键。`--activate` 只切 Slate 的窗口簿记（**不抢操作系统前台**）；`--foreground` 才会真抢并把焦点还回去。**Python 拿不到 EnhancedInput 子系统，驱动坦克只能靠它** |
| `ue_bridge.py` | 早期的文件桥客户端（已被 `editor.deno.ts exec/execfile` 取代，留作参考） |
| `split_tank_mesh.py` | 离线处理 ztz-88a 网格的脚本（不依赖编辑器） |

## probe/（一次性 API 探针）

`probe_pie_api.py`（PIE 相关接口）、`probe_drive_api.py`（驱动/孤儿相关）、
`probe_input_inject.py` + `probe_input_routes2.py`（**Enhanced Input 注入路线，结论：Python 侧不可达**）、
`probe_ia_assets.py` + `probe_ia_assets2.py`（**`EditorAssetLibrary` 在本工程失效，要用 `unreal.load_asset`**）、
`probe_turret_read.py`（炮塔朝向的 Python 读法）。

留着是为了「下次别再花时间试一遍」，结论已固化在 `.workbuddy/memory/MEMORY.md`。

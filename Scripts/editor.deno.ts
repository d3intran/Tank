// 编辑器接管脚本：open / close / status Tank.uproject
// 用法：deno run -A Scripts/editor.deno.ts <status|open|close> [--force] [--no-save] [--timeout 秒]
// close 默认走文件桥"保存脏包→QUIT_EDITOR"优雅退出；桥不可用降级 WM_CLOSE；仅 --force 强杀。

const EDITOR_EXE = "E:\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor.exe";
const PROJECT = "E:\\UE\\Tank\\Tank.uproject";
const BRIDGE_QUEUE = "E:\\UE\\Tank\\Bridge\\queue";
const BRIDGE_RESULTS = "E:\\UE\\Tank\\Bridge\\results";
const MCP_PORT = 8000;

const decoder = new TextDecoder();

async function ps(script: string): Promise<string> {
  const cmd = new Deno.Command("powershell.exe", {
    args: ["-NoProfile", "-NonInteractive", "-Command", script],
    stdin: "null",
  });
  const { stdout, stderr, success } = await cmd.output();
  if (!success) throw new Error(`powershell 失败: ${decoder.decode(stderr).slice(0, 300)}`);
  return decoder.decode(stdout).trim();
}

interface Proc { pid: number }

async function findProcs(): Promise<Proc[]> {
  const out = await ps(
    `Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | ` +
    `Where-Object { $_.CommandLine -like '*Tank.uproject*' } | ` +
    `Select-Object -Property ProcessId | ConvertTo-Json -Compress`,
  );
  if (!out) return [];
  try {
    const parsed = JSON.parse(out);
    if (Array.isArray(parsed)) return parsed.map((p) => ({ pid: p.ProcessId }));
    return [{ pid: parsed.ProcessId }];
  } catch {
    return [];
  }
}

async function anyUnrealEditor(): Promise<boolean> {
  const out = await ps(
    `(Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) -ne $null`,
  );
  return out === "True";
}

async function portOpen(port = MCP_PORT): Promise<boolean> {
  try {
    const c = await Deno.connect({ hostname: "127.0.0.1", port, transport: "tcp" });
    c.close();
    return true;
  } catch {
    return false;
  }
}

async function withTimeout<T>(p: Promise<T>, ms: number): Promise<T> {
  return await Promise.race([
    p,
    new Promise<T>((_, rej) => setTimeout(() => rej(new Error("timeout")), ms)),
  ]);
}

// —— 文件桥：在编辑器内执行 Python（协议见 Content/Python/init_unreal.py）——
// 代码以 base64 传输，规避 bash→deno 的参数转义损耗（git-bash 吃引号实测坑）
async function bridgeExecFile(pyFile: string, timeoutSec = 30): Promise<string> {
  const code = await Deno.readTextFile(pyFile);
  const b64 = btoa(String.fromCharCode(...new TextEncoder().encode(code)));
  const id = crypto.randomUUID().slice(0, 8);
  const queueFile = `${BRIDGE_QUEUE}\\${id}.json`;
  const resultFile = `${BRIDGE_RESULTS}\\${id}.json`;
  const wrapped = `exec(__import__("base64").b64decode("${b64}").decode("utf-8"))`;
  await Deno.writeTextFile(queueFile, JSON.stringify({ type: "py", command: wrapped }));
  const start = Date.now();
  while ((Date.now() - start) / 1000 < timeoutSec) {
    try {
      const raw = JSON.parse(await Deno.readTextFile(resultFile));
      await Deno.remove(resultFile);
      if (raw.ok === false) {
        return `PY-ERROR: ${raw.error ?? JSON.stringify(raw)}`;
      }
      return String(raw.result ?? JSON.stringify(raw));
    } catch { /* 结果未就绪 */ }
    await new Promise((r) => setTimeout(r, 300));
  }
  try { await Deno.remove(queueFile); } catch { /* 已被消费 */ }
  throw new Error(`桥执行超时(${timeoutSec}s)`);
}

async function bridgeExec(code: string, timeoutSec = 30): Promise<string> {
  const id = crypto.randomUUID().slice(0, 8);
  const queueFile = `${BRIDGE_QUEUE}\\${id}.json`;
  const resultFile = `${BRIDGE_RESULTS}\\${id}.json`;
  await Deno.writeTextFile(queueFile, JSON.stringify({ type: "py", command: code }));
  const start = Date.now();
  while ((Date.now() - start) / 1000 < timeoutSec) {
    try {
      const raw = JSON.parse(await Deno.readTextFile(resultFile));
      await Deno.remove(resultFile);
      if (raw.ok === false) {
        return `PY-ERROR: ${raw.error ?? JSON.stringify(raw)}`;
      }
      return String(raw.result ?? JSON.stringify(raw));
    } catch { /* 结果未就绪 */ }
    await new Promise((r) => setTimeout(r, 300));
  }
  try { await Deno.remove(queueFile); } catch { /* 已被消费 */ }
  throw new Error(`桥执行超时(${timeoutSec}s)`);
}

async function cmdStatus(): Promise<number> {
  const procs = await findProcs();
  const port = await portOpen();
  console.log(`Tank 编辑器进程: ${procs.length === 0 ? "无" : procs.map((p) => p.pid).join(", ")}`);
  console.log(`MCP 端口 ${MCP_PORT}: ${port ? "在线（编辑器就绪）" : "离线"}`);
  if (procs.length === 0) {
    if (await anyUnrealEditor()) console.log("（有其他项目的 UnrealEditor 在运行）");
    return 0;
  }
  if (!port) console.log("（进程在但 MCP 未就绪，可能还在启动中）");
  return 0;
}

async function cmdOpen(flags: Flags): Promise<number> {
  const procs = await findProcs();
  if (procs.length > 0) {
    console.log(`编辑器已在运行（PID ${procs.map((p) => p.pid).join(", ")}），无需重复打开。`);
    return 0;
  }
  if (await anyUnrealEditor()) console.log("注意：另一个 UE 项目编辑器在运行，将新开实例。");
  console.log("启动 Tank 编辑器…");

  const child = new Deno.Command(EDITOR_EXE, {
    args: [PROJECT],
    detached: true,
    stdin: "null",
    stdout: "null",
    stderr: "null",
  }).spawn();
  child.unref();
  console.log(`已拉起进程（PID ${child.pid}）。`);

  if (flags.nowait) return 0;

  const timeoutSec = flags.timeout;
  const start = Date.now();
  while ((Date.now() - start) / 1000 < timeoutSec) {
    if ((await findProcs()).length === 0) {
      console.error("编辑器进程退出——启动失败（查日志 %LOCALAPPDATA%\\UnrealEditor\\Saved\\Logs）。");
      return 1;
    }
    if (await portOpen()) {
      console.log(`编辑器就绪（MCP ${MCP_PORT} 在线，耗时 ${Math.round((Date.now() - start) / 1000)}s）。`);
      return 0;
    }
    await new Promise((r) => setTimeout(r, 2000));
    if ((Date.now() - start) % 10000 < 2000) {
      console.log(`  …启动中 ${Math.round((Date.now() - start) / 1000)}s`);
    }
  }
  console.error(`等待就绪超时（${timeoutSec}s），进程仍在——可能首次启动极慢或卡在弹窗。`);
  return 1;
}

async function cmdClose(flags: Flags): Promise<number> {
  let procs = await findProcs();
  if (procs.length === 0) {
    console.log("编辑器未在运行。");
    return 0;
  }

  // 路径 A：文件桥保存 + 优雅退出
  if (!flags.noSave && !flags.force && await portOpen()) {
    console.log("经文件桥保存脏包并退出…");
    try {
      const code =
        `import unreal\n` +
        `saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)\n` +
        `out.append("save:%s" % saved)\n` +
        `unreal.SystemLibrary.execute_console_command(` +
        `unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), "QUIT_EDITOR")\n` +
        `out.append("quit:sent")\n`;
      const res = await withTimeout(bridgeExec(code, 30), 35000);
      console.log(`桥返回: ${res.replace(/\n/g, " ")}`);
    } catch (e) {
      console.log(`桥不可用（${(e as Error).message}），降级 WM_CLOSE。`);
    }
    // 等进程退出
    const start = Date.now();
    while ((Date.now() - start) / 1000 < 60) {
      procs = await findProcs();
      if (procs.length === 0) {
        console.log("编辑器已优雅退出。");
        return 0;
      }
      await new Promise((r) => setTimeout(r, 1000));
    }
  }

  // 路径 B：WM_CLOSE
  procs = await findProcs();
  if (procs.length > 0 && !flags.force) {
    console.log("发送 WM_CLOSE（CloseMainWindow）…");
    for (const p of procs) {
      await ps(`(Get-Process -Id ${p.pid}).CloseMainWindow() | Out-Null`);
    }
    const start = Date.now();
    while ((Date.now() - start) / 1000 < 25) {
      procs = await findProcs();
      if (procs.length === 0) {
        console.log("编辑器已退出。");
        return 0;
      }
      await new Promise((r) => setTimeout(r, 1000));
    }
    console.error("编辑器 25s 未退出——大概率弹了保存对话框。到前台处理一下，或用 --force 强杀（会丢未保存修改）。");
    return 1;
  }

  // 路径 C：强杀
  procs = await findProcs();
  if (procs.length > 0) {
    if (!flags.force) {
      console.error("仍在运行；按策略不强杀，需要时加 --force。");
      return 1;
    }
    console.log("强制结束进程…");
    for (const p of procs) {
      await ps(`Stop-Process -Id ${p.pid} -Force`);
    }
    await new Promise((r) => setTimeout(r, 1500));
    procs = await findProcs();
    console.log(procs.length === 0 ? "已强杀。" : "仍有残留进程。");
    return procs.length === 0 ? 0 : 1;
  }
  return 0;
}

interface Flags { force: boolean; noSave: boolean; nowait: boolean; timeout: number }

function parseFlags(args: string[]): Flags {
  const f: Flags = { force: false, noSave: false, nowait: false, timeout: 600 };
  for (const a of args) {
    if (a === "--force") f.force = true;
    else if (a === "--no-save") f.noSave = true;
    else if (a === "--nowait") f.nowait = true;
    else if (a.startsWith("--timeout=")) f.timeout = Number(a.slice(10)) || f.timeout;
  }
  return f;
}

const [cmd, ...rest] = Deno.args;
const flags = parseFlags(rest);
switch (cmd) {
  case "status":
    Deno.exit(await cmdStatus());
    break;
  case "open":
    Deno.exit(await cmdOpen(flags));
    break;
  case "close":
    Deno.exit(await cmdClose(flags));
    break;
  case "exec": {
    // 直接在编辑器内执行 Python（文件桥）。用法：editor.deno.ts exec "<python 代码>"
    // 桥约定：结果收集进 out 列表；严禁对 out 重新赋值，开头清空用 out.clear()
    if (!(await portOpen())) {
      console.error("编辑器未就绪（端口 8000 离线）。");
      Deno.exit(1);
    }
    const code = rest.join(" ");
    if (!code) {
      console.error('用法: editor.deno.ts exec "<python 代码>"');
      Deno.exit(2);
    }
    try {
      const res = await bridgeExec(code, flags.timeout);
      console.log(res);
      Deno.exit(0);
    } catch (e) {
      console.error(`exec 失败: ${(e as Error).message}`);
      Deno.exit(1);
    }
    break;
  }
  case "execfile": {
    // 推荐：从 .py 文件执行（零转义损耗）。用法：editor.deno.ts execfile Scripts/xxx.py
    if (!(await portOpen())) {
      console.error("编辑器未就绪（端口 8000 离线）。");
      Deno.exit(1);
    }
    if (!rest[0]) {
      console.error("用法: editor.deno.ts execfile <path/to/script.py>");
      Deno.exit(2);
    }
    try {
      const res = await bridgeExecFile(rest[0], flags.timeout);
      console.log(res);
      Deno.exit(0);
    } catch (e) {
      console.error(`execfile 失败: ${(e as Error).message}`);
      Deno.exit(1);
    }
    break;
  }
  default:
    console.log('用法: deno run -A Scripts/editor.deno.ts <status|open|close|exec> [--force] [--no-save] [--nowait] [--timeout=秒]\nexec 示例: editor.deno.ts exec "out.append(unreal.SystemLibrary.get_engine_version())" --timeout=30');
    Deno.exit(2);
}

// Tank 项目构建脚本：UBT 优先，进程拉起被拦截（740/9006）时自动降级为
// 手动 rsp 编译链接链。用法：
//   deno run -A build.deno.ts Development      （或 DebugGame，默认 Development）
// 编辑器运行时：编译可进行，但链接会因 DLL 占用跳过并提示。
import { delay } from "https://deno.land/std@0.224.0/async/delay.ts";

const ENGINE = "E:/UE_5.8";
const PROJECT = "E:/UE/Tank";
const VARIANT = Deno.args[0] === "DebugGame" ? "DebugGame" : "Development";
const MSVC = "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64";
const OBJ_DIR = `${PROJECT}/Intermediate/Build/Win64/x64/UnrealEditor/${VARIANT}/Tank`;
const DLL_NAME = VARIANT === "DebugGame" ? "UnrealEditor-Tank-Win64-DebugGame" : "UnrealEditor-Tank";
const ENV_FALLBACK = { ...Deno.env.toObject(), MSYS2_ARG_CONV_EXCL: "*", MSYS_NO_PATHCONV: "1" };

async function editorRunning(): Promise<boolean> {
  try {
    const res = await fetch("http://127.0.0.1:8000/mcp", { method: "HEAD", signal: AbortSignal.timeout(1500) });
    return res.status === 405;
  } catch {
    return false;
  }
}

async function sh(cmd: string[], cwd?: string): Promise<number> {
  const c = new Deno.Command(cmd[0], { args: cmd.slice(1), cwd, env: ENV_FALLBACK, stdout: "piped", stderr: "piped" });
  const out = await c.output();
  const text = new TextDecoder().decode(out.stdout);
  const tail = text.trim().split("\n").slice(-6).join("\n");
  console.log(tail);
  return out.code;
}

async function compileAllRsp(): Promise<void> {
  // 全量重编模块（带 obj 新旧判断的增量交给 UBT 常规路径；这里是兜底路径）
  for (const dir of [OBJ_DIR, `${PROJECT}/Intermediate/Build/Win64/x64/TankEditor/${VARIANT}`]) {
    try {
      for await (const f of Deno.readDir(dir)) {
        if (!f.name.endsWith(".obj.rsp")) continue;
        const rsp = `${dir}/${f.name}`;
        const obj = rsp.replace(/\.rsp$/, "");
        const rspStat = await Deno.stat(rsp);
        let stale = true;
        try {
          stale = (await Deno.stat(obj)).mtime! < rspStat.mtime!;
        } catch { /* obj 不存在 */ }
        if (!stale) continue;
        console.log(`[fallback] cl ${f.name.replace(".obj.rsp", "")}`);
        await sh([`${MSVC}/cl.exe`, `@${rsp}`], `${ENGINE}/Engine/Source`);
      }
    } catch { /* 目录不存在则跳过 */ }
  }
}

async function writeMetadata(): Promise<void> {
  const input = `${PROJECT}/Intermediate/Build/Win64/x64/TankEditor/${VARIANT}/TargetMetadata.json`;
  try {
    await Deno.stat(input);
  } catch {
    return;
  }
  console.log("[fallback] WriteMetadata");
  await sh([
    `${ENGINE}/Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe`,
    `${ENGINE}/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll`,
    `-Session={${crypto.randomUUID()}}`, "-Mode=WriteMetadata",
    `-Input=${input}`, "-Version=2",
  ], `${ENGINE}/Engine/Source`);
}

// 1. 编辑器检测
if (await editorRunning()) {
  console.log("!! UE 编辑器正在运行（端口 8000 有响应）：DLL 被锁定，无法链接。");
  console.log("   关闭编辑器后重跑本脚本。");
  Deno.exit(2);
}

// 2. 常规 UBT
console.log(`=== UBT ${VARIANT} ===`);
const ubtCode = await sh([
  "cmd", "/c", `${ENGINE}/Engine/Build/BatchFiles/Build.bat`, "TankEditor", "Win64", VARIANT,
  `-project=${PROJECT}/Tank.uproject`, "-WaitMutex",
]);
if (ubtCode === 0) {
  console.log("UBT 构建成功。");
  Deno.exit(0);
}

// 3. 兜底：手动编译 → 链接 DLL → 导入库 → 元数据
console.log("=== UBT 失败，走手动 fallback ===");
await compileAllRsp();
await sh([`${MSVC}/link.exe`, `@${OBJ_DIR}/${DLL_NAME}.dll.rsp`], `${ENGINE}/Engine/Source`);
await sh([`${MSVC}/link.exe`, "/LIB", `@${OBJ_DIR}/${DLL_NAME}.lib.rsp`], `${ENGINE}/Engine/Source`);
await writeMetadata();
console.log("fallback 构建完成。");

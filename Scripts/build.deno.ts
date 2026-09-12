// Tank 项目构建脚本：UBT 优先，进程拉起被拦截（740/9006）时自动降级为
// 手动 rsp 编译链接链。用法：
//   deno run -A build.deno.ts Development      （或 DebugGame，默认 Development）
//   deno run -A build.deno.ts Development      # 需在沙箱外跑（UBT/UBA 要写 ProgramData、AppData）
// 编辑器运行时：编译可进行，但链接会因 DLL 占用跳过并提示。
// 改了 UFUNCTION/UPROPERTY 后必须先让 UBT 跑一轮生成 UHT 代码（直接 cl 会报 C2511）。
// 失败时会把 MSVC(走 stderr) 与 UBT(走 stdout) 的诊断一起打出来，不再只留一句 exit code。
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
  const dec = new TextDecoder();
  // MSVC 的诊断走 stderr，UBT 的走 stdout —— 两边都要收，否则失败时只剩一句 exit code
  const lines = (dec.decode(out.stdout) + "\n" + dec.decode(out.stderr))
    .split("\n").filter((l) => l.trim());
  const errs = lines.filter((l) => /error [A-Z]?\d+|error:|LNK\d+|fatal error|: error/i.test(l));
  if (errs.length) {
    console.log(errs.slice(0, 40).join("\n"));
    if (errs.length > 40) console.log(`  …共 ${errs.length} 行诊断`);
  } else {
    console.log(lines.slice(-6).join("\n"));
  }
  return out.code;
}

/** 从 cl 的 rsp 里取输入源文件（第一行就是绝对路径）。取不到返回 null。 */
async function rspSource(rsp: string): Promise<string | null> {
  try {
    const first = (await Deno.readTextFile(rsp)).split("\n")[0].trim().replace(/^"|"$/g, "");
    return first.toLowerCase().endsWith(".cpp") ? first : null;
  } catch {
    return null;
  }
}

/** obj 是否过期。rsp 本身不会因源码改动而更新，所以必须比源文件时间；
 *  头文件更是不出现在 rsp 里，用「Source/Tank 下最新文件」兜底（宁多编不漏编）。 */
async function needsCompile(rsp: string, obj: string): Promise<boolean> {
  let objTime: number;
  try {
    objTime = (await Deno.stat(obj)).mtime!.getTime();
  } catch {
    return true; // obj 不存在
  }
  const src = await rspSource(rsp);
  if (src) {
    try {
      if ((await Deno.stat(src)).mtime!.getTime() > objTime) return true;
    } catch { /* 源已不存在：由调用方跳过 */ }
  }
  if ((await Deno.stat(rsp)).mtime!.getTime() > objTime) return true;

  let newest = 0;
  try {
    for await (const f of Deno.readDir(`${PROJECT}/Source/Tank`)) {
      if (!f.isFile) continue;
      const m = (await Deno.stat(`${PROJECT}/Source/Tank/${f.name}`)).mtime!.getTime();
      if (m > newest) newest = m;
    }
  } catch { /* 忽略 */ }
  return newest > objTime;
}

async function compileAllRsp(): Promise<void> {
  // 全量重编模块（带 obj 新旧判断的增量交给 UBT 常规路径；这里是兜底路径）
  for (const dir of [OBJ_DIR, `${PROJECT}/Intermediate/Build/Win64/x64/TankEditor/${VARIANT}`]) {
    try {
      for await (const f of Deno.readDir(dir)) {
        if (!f.name.endsWith(".obj.rsp")) continue;
        const rsp = `${dir}/${f.name}`;
        const obj = rsp.replace(/\.rsp$/, "");
        // 陈旧中间产物：rsp 指向的源文件早已删除（本项目残留过 ClimbManager/Def*/Zombie* 等）。
        // 直接跳过，否则编译必然以 C1083「无法打开源文件」失败并中断整轮构建。
        const src = await rspSource(rsp);
        if (src) {
          try {
            await Deno.stat(src);
          } catch {
            console.log(`[fallback] 跳过陈旧 rsp（源已不存在）: ${f.name}`);
            continue;
          }
        }
        if (!(await needsCompile(rsp, obj))) continue;
        console.log(`[fallback] cl ${f.name.replace(".obj.rsp", "")}`);
        const code = await sh([`${MSVC}/cl.exe`, `@${rsp}`], `${ENGINE}/Engine/Source`);
        if (code !== 0) {
          console.error("编译失败，终止（上面是诊断）。");
          Deno.exit(1);
        }
      }
    } catch { /* 目录不存在则跳过 */ }
  }
}

async function writeMetadata(): Promise<void> {  const input = `${PROJECT}/Intermediate/Build/Win64/x64/TankEditor/${VARIANT}/TargetMetadata.json`;
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

/** UBT 的链接失败会留下半成品：把导入库和 PDB 截断成 0 字节、旧 DLL 也已被删。
 *  下一次构建若不先清掉，兜底路径会拿这个 0 字节 .lib 去链接，报
 *  `LNK1136: invalid or corrupt file`（实测就是这么被卡的）。开跑前统一清一遍。 */
async function cleanCorruptOutputs(): Promise<void> {
  for (const p of [
    `${OBJ_DIR}/${DLL_NAME}.lib`,
    `${OBJ_DIR}/${DLL_NAME}.exp`,
    `${PROJECT}/Binaries/Win64/${DLL_NAME}.pdb`,
  ]) {
    try {
      if ((await Deno.stat(p)).size === 0) {
        await Deno.remove(p);
        console.log(`[clean] 删除 0 字节产物 ${p}`);
      }
    } catch { /* 不存在则跳过 */ }
  }
}

// 1. 编辑器检测
if (await editorRunning()) {
  console.log("!! UE 编辑器正在运行（端口 8000 有响应）：DLL 被锁定，无法链接。");
  console.log("   关闭编辑器后重跑本脚本。");
  Deno.exit(2);
}

// 2. 常规 UBT
await cleanCorruptOutputs();
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
await cleanCorruptOutputs();
await compileAllRsp();
// 顺序要紧：先出导入库（/LIB），再链接 DLL —— DLL 链接会按 /IMPLIB 覆写导入库，
// 反序会让 lib 落后于 dll，其他模块按旧符号表链接
if (await sh([`${MSVC}/link.exe`, "/LIB", `@${OBJ_DIR}/${DLL_NAME}.lib.rsp`], `${ENGINE}/Engine/Source`) !== 0) {
  console.error("导入库链接失败。");
  Deno.exit(1);
}
if (await sh([`${MSVC}/link.exe`, `@${OBJ_DIR}/${DLL_NAME}.dll.rsp`], `${ENGINE}/Engine/Source`) !== 0) {
  console.error("DLL 链接失败。");
  Deno.exit(1);
}
await writeMetadata();
console.log("fallback 构建完成。");

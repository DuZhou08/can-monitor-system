#!/usr/bin/env python3
"""Build an EIDE-managed STM32 project with arm-none-eabi-gcc.

Usage:  python3 scripts/build.py <project-dir>

Reads <project-dir>/.eide/eide.yml (source list + compiler options) and builds
<project-dir>/build/ci/<project>.elf .  No EIDE/VS Code required.
"""
import os, sys, glob, subprocess, shutil

try:
    import yaml
except ImportError:
    sys.exit("PyYAML required: pip install pyyaml")

CC   = os.environ.get("CC", "arm-none-eabi-gcc")
SIZE = os.environ.get("SIZE", "arm-none-eabi-size")

OPT_MAP = {"level-0": "-O0", "level-debug": "-Og", "level-1": "-O1",
           "level-2": "-O2", "level-3": "-O3", "level-s": "-Os"}


def load(proj):
    cfg = yaml.safe_load(open(os.path.join(proj, ".eide", "eide.yml"), encoding="utf-8"))
    tname = list(cfg["targets"].keys())[0]
    tgt = cfg["targets"][tname]
    gcc = tgt["toolchainConfigMap"]["GCC"]
    return cfg, tgt, gcc


def collect_sources(proj, cfg):
    files = []

    def walk(folder):
        for f in folder.get("files", []):
            p = f.get("path", "")
            if p.endswith((".c", ".S", ".s", ".cpp")):
                files.append(p)
        for sub in folder.get("folders", []):
            walk(sub)

    walk(cfg.get("virtualFolder", {}))
    for d in cfg.get("srcDirs", []):
        for p in glob.glob(os.path.join(proj, d, "**", "*.c"), recursive=True):
            files.append(os.path.relpath(p, proj))

    seen, out = set(), []
    for f in files:
        if f not in seen and os.path.isfile(os.path.join(proj, f)):
            seen.add(f); out.append(f)
    return out


def cflags(proj, tgt, gcc):
    o = gcc["options"]
    c = o.get("c/cpp-compiler", {})
    g = o.get("global", {})
    f = []
    cpu = gcc.get("cpuType", "Cortex-M3")
    f.append("-mcpu=cortex-m4" if cpu == "Cortex-M4" else "-mcpu=cortex-m3")
    f.append("-mthumb")
    fp = gcc.get("floatingPointHardware", "none")
    if fp and fp != "none":
        f.append("-mfpu=fpv4-sp-d16")
        f.append("-mfloat-abi=hard" if g.get("$float-abi-type") == "hard" else "-mfloat-abi=softfp")
    else:
        f.append("-mfloat-abi=soft")
    f.append(OPT_MAP.get(c.get("optimization", "level-0"), "-O0"))
    if c.get("warnings") == "all-warnings":
        f.append("-Wall")
    if c.get("one-elf-section-per-function"):
        f.append("-ffunction-sections")
    if c.get("one-elf-section-per-data"):
        f.append("-fdata-sections")
    if c.get("language-c"):
        f.append("-std=" + c["language-c"])
    if g.get("output-debug-info") == "enable":
        f.append("-g")
    if g.get("not-use-syscalls"):
        f.append("--specs=nosys.specs")
    if g.get("use-newlib-nano"):
        f.append("--specs=nano.specs")
    for d in tgt["cppPreprocessAttrs"].get("defineList", []):
        f.append("-D" + str(d))
    for inc in tgt["cppPreprocessAttrs"].get("incList", []):
        f.append("-I" + os.path.join(proj, inc))
    for extra in (c.get("C_FLAGS") or "").split():
        f.append(extra)
    return f


def ldflags(proj, gcc):
    o = gcc["options"]
    lk = o.get("linker", {})
    f = []
    if lk.get("remove-unused-input-sections"):
        f.append("-Wl,--gc-sections")
    sc = gcc.get("scatterFilePath")
    if sc:
        f.append("-T" + os.path.join(proj, sc))
    for extra in (lk.get("LIB_FLAGS") or "").split():
        f.append(extra)
    return f


def build(proj):
    cfg, tgt, gcc = load(proj)
    srcs = collect_sources(proj, cfg)
    out = os.path.join(proj, "build", "ci")
    objdir = os.path.join(out, "obj")
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(objdir, exist_ok=True)
    cfl = cflags(proj, tgt, gcc)
    ldl = ldflags(proj, gcc)
    name = os.path.basename(os.path.normpath(proj))
    objs = []
    print(f"[{name}] {len(srcs)} source files, flags: {' '.join(cfl)}")
    for s in srcs:
        obj = os.path.join(objdir, s.replace("/", "_") + ".o")
        os.makedirs(os.path.dirname(obj), exist_ok=True)
        cmd = [CC] + cfl + ["-c", os.path.join(proj, s), "-o", obj]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout); print(r.stderr)
            sys.exit(f"[{name}] FAILED compiling {s}")
        objs.append(obj)
    elf = os.path.join(out, name + ".elf")
    cmd = [CC] + cfl + objs + ldl + ["-o", elf, "-Wl,-Map=" + os.path.join(out, name + ".map")]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout); print(r.stderr)
        sys.exit(f"[{name}] FAILED linking")
    print(f"[{name}] OK -> {elf}")
    subprocess.run([SIZE, elf])


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    build(sys.argv[1])

Import("env")

import os
import shutil


if os.name == "nt" and shutil.which("g++", path=env["ENV"].get("PATH")) is None:
    toolchain_dir = os.path.join(
        env.subst("$PROJECT_CORE_DIR"),
        "packages",
        "toolchain-gccmingw32",
    )
    toolchain_bin = os.path.join(toolchain_dir, "bin")
    if not os.path.isdir(toolchain_bin):
        raise RuntimeError(
            "PlatformIO managed native GCC toolchain was not found: "
            + toolchain_bin
        )
    env.PrependENVPath("PATH", toolchain_bin)
    env.Append(
        LINKFLAGS=["-static", "-static-libgcc", "-static-libstdc++"]
    )

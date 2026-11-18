"""PlatformIO pre-script: optional explicit release ID, validated before compilation."""

import os
import re

globals()["Import"]("env")  # PlatformIO/SCons injects Import and env
build = os.environ.get("PICKLESCORE_BUILD_ID")
# Spec 04 §§7/9: give the nonconfirming fixture a distinguishable, bench-only identity.
rollback_test = globals()["env"]["PIOENV"] == "ota_rollback_test"
if rollback_test != (build or "").startswith("bench-rollback-"):
    raise ValueError("Only ota_rollback_test uses IDs starting with bench-rollback-")
if build:
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,31}", build):
        raise ValueError("Invalid PICKLESCORE_BUILD_ID (1..31 safe ASCII characters)")
    globals()["env"].Append(
        CPPDEFINES=[("PICKLESCORE_BUILD_ID", '\\"' + build + '\\"')]
    )

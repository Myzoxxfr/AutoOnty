#!/usr/bin/env python3
"""

AutoOnty - Version Bumper

Updates the version number everywhere it appears, in one go:
  1. src/core/update.cpp   -> APP_VERSION, CUR_VERSION, user agent string
  2. src/app.rc            -> FileVersion / ProductVersion
  3. version               -> number the update checker reads from GitHub
  4. README.md             -> version badge, release link, comparison table

Usage:
  Double-click it (or run `python scripts/bump_version.py`) and type the new
  version, e.g. 1.2.3
  Or pass it directly: python scripts/bump_version.py 1.2.3

"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def path(*parts):
    return os.path.join(ROOT, *parts)


def read(p):
    # bytes in / bytes out so BOMs and line endings are never touched
    with open(p, "rb") as f:
        return f.read().decode("utf-8")


def write(p, text):
    with open(p, "wb") as f:
        f.write(text.encode("utf-8"))


def parse(v):
    m = re.fullmatch(r"v?(\d+)\.(\d+)(?:\.(\d+))?", v.strip())
    if not m:
        return None
    major, minor, patch = (int(x) if x else 0 for x in m.groups())
    if major > 999 or minor > 999 or patch > 999:
        return None
    return major, minor, patch


def short(t):
    # 1.2.0 is shown as 1.2 (like v1.2), anything else as 1.2.3
    return f"{t[0]}.{t[1]}" if t[2] == 0 else f"{t[0]}.{t[1]}.{t[2]}"


def current_version():
    t = read(path("src", "core", "update.cpp"))
    m = re.search(r'APP_VERSION\s*=\s*L"v([^"]+)"', t)
    if not m:
        raise RuntimeError("Couldn't find APP_VERSION in src/core/update.cpp")
    cur = parse(m.group(1))
    if not cur:
        raise RuntimeError(f"Couldn't understand current version '{m.group(1)}'")
    return m.group(1), cur


def sub(text, pattern, repl, label, expect_min=1):
    new, n = re.subn(pattern, repl, text)
    if n < expect_min:
        raise RuntimeError(f"Couldn't find {label}")
    return new, n


def apply(old_str, new):
    new_str = short(new)
    num = new[0] * 1000000 + new[1] * 1000 + new[2]
    full4 = f"{new[0]}.{new[1]}.{new[2]}.0"
    changes = []

    # --- update.cpp ---
    p = path("src", "core", "update.cpp")
    t = read(p)
    t, _ = sub(t, r'(APP_VERSION\s*=\s*L")v[^"]+(")', rf"\g<1>v{new_str}\g<2>",
               "APP_VERSION")
    t, _ = sub(t, r"(CUR_VERSION\s*=\s*)\d+", rf"\g<1>{num}", "CUR_VERSION")
    t, _ = sub(t, r'(L"AutoOnty/)[^"]+(")', rf"\g<1>{new_str}\g<2>",
               "user agent string")
    write(p, t)
    changes.append("src/core/update.cpp (APP_VERSION, CUR_VERSION, user agent)")

    # --- app.rc ---
    p = path("src", "app.rc")
    t = read(p)
    t, _ = sub(t, r'(VALUE "FileVersion",\s*")[^"]+(")', rf"\g<1>{full4}\g<2>",
               "FileVersion", 1)
    t, _ = sub(t, r'(VALUE "ProductVersion",\s*")[^"]+(")', rf"\g<1>{full4}\g<2>",
               "ProductVersion", 1)
    write(p, t)
    changes.append("src/app.rc (FileVersion, ProductVersion)")

    # --- version file ---
    write(path("version"), f"{num}\n")
    changes.append(f"version ({num})")

    # --- README ---
    p = path("README.md")
    t = read(p)
    t = t.replace(f"v{old_str}", f"v{new_str}")
    write(p, t)
    changes.append("README.md (badge, release link, comparison table)")

    return changes


def run(arg):
    old_str, cur = current_version()
    print(f"Current version: v{old_str}")

    if arg is None:
        arg = input("New version (e.g. 1.2.3): ")
    new = parse(arg)
    if not new:
        print(f"'{arg.strip()}' isn't a valid version. Use something like 1.2.3")
        return 1
    if new == cur:
        print("That's already the current version.")
        return 1
    if new < cur:
        ans = input(f"v{short(new)} is LOWER than v{old_str}. Continue anyway? (y/N) ")
        if ans.strip().lower() != "y":
            print("Cancelled.")
            return 1

    print(f"\nv{old_str}  ->  v{short(new)}")
    if len(sys.argv) < 2:
        ans = input("Apply? (Y/n) ")
        if ans.strip().lower() == "n":
            print("Cancelled.")
            return 1

    for c in apply(old_str, new):
        print("  updated", c)

    print(
        "\nDone. Reminder: the update checker reads the `version` file from GitHub's"
        "\nmain branch, so only push it once the v" + short(new) + " release is actually up."
    )
    return 0


def main():
    arg = sys.argv[1] if len(sys.argv) > 1 else None
    try:
        code = run(arg)
    except Exception as e:  # keep the window open when double-clicked
        print("Error:", e)
        code = 1
    if len(sys.argv) < 2:
        input("\nPress Enter to close...")
    sys.exit(code)


if __name__ == "__main__":
    main()

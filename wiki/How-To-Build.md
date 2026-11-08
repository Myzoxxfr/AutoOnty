# Building from Source

Build AutoOnty with Visual Studio C++ tools into a single executable (about 888 KB, depending on build options).

---

## 1. What You Need

- **Operating System:** Windows 10 or Windows 11.
- **Compiler:** Visual Studio with the **Desktop development with C++** workload and C++20 support.
- **Project Toolset:** The project is configured with MSVC toolset (`v145`) and Windows 10 SDK. If your Visual Studio setup uses a different SDK or toolset version, right-click the project in Visual Studio and select **Retarget solution** or adjust the platform toolset to match your installed version.

---

## 2. Quick Way (Visual Studio)

1. Double-click `AutoOnty.slnx` to open it in Visual Studio.
2. Set the mode to **Release** (and **x64**) in the toolbar.
3. Press <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>B</kbd>.

Your compiled executable will be placed in `bin/x64/AutoOnty.exe`.

---

## 3. Compiling with MSBuild (Optional)

Open **Developer Command Prompt** or **Developer PowerShell for Visual Studio**, navigate to the AutoOnty directory, and run:

```powershell
# Build 64-bit Release version
msbuild AutoOnty.vcxproj /p:Configuration=Release /p:Platform=x64
```

Your compiled executable is `bin/x64/AutoOnty.exe`. Run it directly.


## 4. Build Script and Tests

With the same Visual Studio C++ tools installed, you can also build from PowerShell:

```powershell
./scripts/build-local.ps1
```

This builds Release-style x64 output at `build/features/AutoOnty.exe`. Close AutoOnty before rebuilding its executable.

Run the automated checks with:

```powershell
./scripts/test.ps1
```

Tests cover parsing, playback, timing, editing, settings and images. They also run in GitHub CI. Generated files go in `build/tests/` and are not part of the app.

## Image matching checks

Image matching uses Win32 capture and GDI+ image import, with no additional image libraries. The test script above includes the image checks.

Use `./scripts/test.ps1 -Suite Benchmark` for image-matching performance checks or `-Suite Capture` for capture checks. To export dark/light editor previews, set `$env:AUTOONTY_UI_PREVIEWS = "1"` before running the Editor suite. Tests do not snip or click the desktop.

## Version bump check

After running `scripts/bump_version.py`, run the version regression check with Python:

```powershell
python -B -m unittest discover -s tests -p test_bump_version.py -v
```

This checks application/resource versions and preserves source encoding and the Windows Common Controls dependency version.

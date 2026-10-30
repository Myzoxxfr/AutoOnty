# Building from Source

Want to compile AutoOnty yourself? It is straightforward and builds with Visual Studio tools into a single lightweight binary (about 700 KB, depending on build options).

---

## 1. What You Need

- **Operating System:** Windows 10 or Windows 11 (Windows 7 support is planned).
- **Compiler:** Visual Studio (2022 / 2026) with the **Desktop development with C++** workload (C++20).
- **Project Toolset:** The project is configured with MSVC toolset (`v145`) and Windows 10 SDK. If your Visual Studio setup uses a different SDK or toolset version, simply right-click the project in Visual Studio and select **Retarget solution** or adjust the platform toolset to match your installed version.

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

Your compiled executable will be placed in:

- `bin/x64/AutoOnty.exe`

Just run `AutoOnty.exe` - that's all there is to it!


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

The `tests/` folder contains checks for macro parsing, playback, input timing, editor actions and settings. They run in GitHub CI too and are not included in the released app. Generated test files go in `build/tests/`; `build/` is ignored by Git.

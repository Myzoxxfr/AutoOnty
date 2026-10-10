# Command-Line (CLI) & Automation

AutoOnty supports command-line arguments and single-instance IPC communication for integration with scripts, shortcuts, and scheduled tasks.

---

## Syntax

```bat
AutoOnty.exe [macro.onty] [options]
```

Or with explicit `--file`:

```bat
AutoOnty.exe --file "C:\Macros\farm.onty" --play --loop 5 --speed 2.0
```

---

## Command-Line Options

| Flag               | Short | Description                                              | Example                                 |
| :----------------- | :---: | :------------------------------------------------------- | :-------------------------------------- |
| `[path]`           |   -   | Path to a `.onty` file to load on start                  | `AutoOnty.exe "farm.onty"`              |
| `--file <path>`    | `-f`  | Specifies file path (safe for spaces)                    | `AutoOnty.exe -f "C:\Macros\farm.onty"` |
| `--play`           | `-p`  | Automatically plays macro after loading                  | `AutoOnty.exe "farm.onty" --play`       |
| `--rec`            | `-r`  | Automatically starts recording on startup                | `AutoOnty.exe --rec`                    |
| `--loop <N>`       | `-l`  | Loop count. Use `0` for infinite loop (`∞`)              | `AutoOnty.exe --play --loop 5`          |
| `--speed <N>`      | `-s`  | Speed multiplier (`0.01` to `1000.0` or `turbo`)         | `AutoOnty.exe --play --speed 2.5`       |
| `--min` / `--hide` | `-m`  | Launches AutoOnty minimized to system tray               | `AutoOnty.exe "farm.onty" --min --play` |
| `--close-after`    | `-c`  | Automatically closes AutoOnty when playback ends         | `AutoOnty.exe "farm.onty" --play -c`    |
| `--install`        | `-i`  | Installs AutoOnty to Local AppData & registers shortcuts | `AutoOnty.exe --install`                |

---

## Single-Instance Behavior

If AutoOnty is already open, running a command line does not spawn extra background processes. Arguments are forwarded directly to your active AutoOnty window via Windows IPC messages.

---

## Automation Examples

### 1. One-Click Desktop Shortcut

Create a standard Windows shortcut:

- **Target:** `C:\AutoOnty\AutoOnty.exe "C:\Macros\routine.onty" --play --close-after --min`
- **Run:** Minimized

### 2. PowerShell Script

```powershell
$Onty = "C:\AutoOnty\bin\x64\AutoOnty.exe"
& $Onty "C:\Macros\step1.onty" --play --close-after | Out-Null
Start-Sleep -Seconds 2
& $Onty "C:\Macros\step2.onty" --play --speed 2.0 --loop 3 --close-after | Out-Null
```

### 3. Task Scheduler

In **Task Scheduler** (`taskschd.msc`), create a task with action:

- **Program:** `C:\AutoOnty\AutoOnty.exe`
- **Arguments:** `"C:\Macros\daily.onty" --play --close-after --min`

### 4. Background Infinite Loop

```bat
start "" "C:\AutoOnty\AutoOnty.exe" "monitor.onty" --loop 0 --play --min
```

To stop playback anytime:

- Press a panic key (default <kbd>Pause</kbd>).
- Or click the AutoOnty icon in your system tray!

# The .onty File Format

AutoOnty stores recorded macros in structured JSON format with the `.onty` extension. Because files are plain UTF-8 JSON, they can be inspected, version-controlled, or edited in text editors.

---

## 1. File Structure Example

A `.onty` macro contains top-level playback settings and an array of recorded input events:

```json
{
    "onty": 1,
    "speed": 1,
    "loops": 1,
    "continuous": false,
    "events": [
        {
            "t": 0,
            "e": "kd",
            "vk": 65,
            "ext": 0
        },
        {
            "t": 120,
            "e": "ku",
            "vk": 65,
            "ext": 0
        },
        {
            "t": 350,
            "e": "md",
            "b": 0,
            "x": 1280,
            "y": 720
        },
        {
            "t": 420,
            "e": "mu",
            "b": 0,
            "x": 1280,
            "y": 720
        },
        {
            "t": 600,
            "e": "mv",
            "rel": 1,
            "p": [
                [10, 0, 16],
                [15, -2, 16],
                [8, -5, 16]
            ]
        },
        {
            "t": 850,
            "e": "wh",
            "d": 120
        }
    ]
}
```

---

## 2. Properties Reference

### Root Properties

- `onty`: Schema version (integer, currently `1`).
- `speed`: Playback speed multiplier (e.g. `1` for normal, `2` for 2x, `100` for turbo, or decimal numbers like `1.5`).
- `loops`: Number of playback repetitions (integer).
- `continuous`: Infinite loop flag (`true` or `false`).

- `events`: Array of sequential input events.

Saving records the macro's effective speed and loop settings in the file. Opening it in the app uses the general Speed / Loops settings or its library overrides (**Settings...**). Library overrides and macro hotkeys are stored in `macros.ini`, not inside the `.onty` file.

A new AutoOnty process started with a file applies its saved playback settings, with `--speed` / `--loop` taking priority. An already running instance keeps its current settings and library overrides. For scripted runs that need the file settings or command-line overrides to take priority, close AutoOnty first. See [CLI Automation](CLI-Automation.md).

### Event Types (`e`)

`t` is the delay in milliseconds **before each event**, not an elapsed timestamp. For movement events, `p` also stores delays for individual movement points.

| Event Type | Description         | Associated Parameters                                                              |
| :--------- | :------------------ | :--------------------------------------------------------------------------------- |
| `"kd"`     | Key Down            | `vk` (virtual-key code), `ext` (extended key flag)                                 |
| `"ku"`     | Key Up              | `vk` (virtual-key code), `ext` (extended key flag)                                 |
| `"md"`     | Mouse Button Down   | `b` (`0`=Left, `1`=Right, `2`=Middle, `3`=X1, `4`=X2), `x`, `y`                    |
| `"mu"`     | Mouse Button Up     | `b` (`0`=Left, `1`=Right, `2`=Middle, `3`=X1, `4`=X2), `x`, `y`                    |
| `"mv"`     | Mouse Motion / Turn | `rel` (`1` for relative delta, `0` for absolute), `p` array of `[dx, dy, delayMs]` |
| `"wh"`     | Mouse Wheel Scroll  | `d` (signed wheel delta), optional `hw: true` for horizontal scrolling                                     |

---

Wheel events without `hw` remain vertical, so old macros keep their existing behavior. With `hw: true`, positive `d` scrolls right and negative scrolls left. Play selected exclusions and per-macro visibility are stored locally in `Documents\AutoOnty\editor-settings.ini`, not in the macro file.

## 3. Windows File Association

AutoOnty automatically registers `.onty` files with Windows Explorer when launched:

- Double-click a `.onty` file in File Explorer to load it, then use Play to run it.
- Associates the custom AutoOnty icon with `.onty` files.

---

## 4. Programmatic Macro Creation

You can generate `.onty` files using Python, PowerShell, or any standard JSON tools:

```python
import json

macro = {
    "onty": 1,
    "speed": 1,
    "loops": 1,
    "continuous": False,
    "events": []
}

for index in range(5):
    # Wait 200 ms between presses; begin the first press immediately.
    macro["events"].append({"t": 0 if index == 0 else 200,
                            "e": "kd", "vk": 65, "ext": 0})
    # Hold 'A' (VK 65) for 50 ms, then release it.
    macro["events"].append({"t": 50, "e": "ku", "vk": 65, "ext": 0})

with open("press_a_5x.onty", "w", encoding="utf-8") as f:
    json.dump(macro, f, indent=4)

print("Macro generated: press_a_5x.onty")
```

## Image recognition events

For setup and usage in macros and autoclicking, see [Image Recognition](Image-Recognition.md).

```json
{"t":100,"e":"image","image":"image-123456.bmp","sensitivity":85,"timeout":0,"action":0,"target":0,"repeat":1,"after_found":0,"after_timeout":0,"interval":100,"cooldown":300,"watch_time":0,"auto_time":0,"max_matches":0,"clicks":10,"click_x":0,"click_y":0,"sizes":false,"hide":true,"standalone":true}
```

- `image`: a content-based relative BMP path in `ImageDetections/`. Legacy BMP basenames beside the macro remain supported; other directories, traversal, absolute paths and reserved device names are rejected. Images are 32-bit RGB; clipboard input also accepts 24-bit RGB and standard RGB bitfields.
- `sensitivity`: integer 0–100, default 85. Preset names are never stored.
- `timeout`: integer 0–4294967295 ms; 0 waits indefinitely. Missing means 3000 (default when omitted).
- `action`: 0 click once (default when omitted), 1 auto click, 2 run copied macro, 3 wait only.
- `target`: 0 image center, 1 current cursor, 2 fixed `click_x`/`click_y` (signed LONG coordinates).
- `repeat`: 0 once, 1 after disappearance, 2 while visible, 3 each location once.
- `after_found` and `after_timeout`: 0 continue, 1 manual pause, 2 stop.
- `interval`: click interval 1–4294967295 ms. `cooldown`, `watch_time`, `auto_time`, `max_matches`, `clicks`: unsigned DWORD values; zero is unlimited for time/count limits.
- `sizes`, `hide`, `standalone`: boolean size search, image-specific auto-minimize and standalone watcher flag. A single standalone image action uses its own match/time limits instead of general playback loops.
- `run`: optional embedded macro JSON object with `onty`/`events`; `run_name` is its display name. Action 2 requires a nonempty valid `run`. It is an immutable copied action, not a file reference. Nested actions are limited to eight levels and 100000 total events.

Missing options use backward-compatible defaults. Invalid numbers, assets, nested actions and enum values reject the load without replacing the current macro. Saved image dimensions are bounded to 16384 per side / 32 million pixels, and shared image assets total at most 64 million pixels. The search desktop has separate bounds of 65536 per side / 64 million pixels.

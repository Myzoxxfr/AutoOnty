# Macro Engine & 3D Games

AutoOnty records keyboard and mouse input for desktop tasks and games, including relative mouse movement for 3D camera controls.

---

## 1. Why 3D Games Require Special Handling

In standard desktop applications, mouse movements and clicks rely on absolute `(X, Y)` screen pixel coordinates.

Many 3D games (such as Roblox, Minecraft, or first-person titles) use relative mouse movement:

1. The game hides and locks the mouse cursor in the center of the screen.
2. The game camera rotates based on relative movement deltas (`dx, dy`) read directly from the mouse hardware.
3. Standard macro tools that only update cursor coordinates fail to rotate the camera when the cursor is locked.

---

## 2. How AutoOnty Handles Input

AutoOnty combines low-level input hooks, Raw Input and movement tracking:

- **Desktop Input Capture:** Records clicks, keystrokes, dragging, and mouse wheel scrolls with millisecond timestamps.
- **Raw Input Stream for 3D Camera:** Records relative mouse deltas delivered by Windows, including movement while a game keeps the cursor centered or locked.
- **High-Resolution Drag Tracking:** Tracks movements while mouse buttons are held so playback can follow the recorded drag path.

---

## 3. High-Precision Timing

AutoOnty sleeps during longer waits and uses a high-resolution timer for shorter waits. Playback timing is best-effort and depends on Windows scheduling and the target application.

---

## 4. Speed Multipliers & Turbo Mode

Set playback speed under **Menu -> Macro settings -> Speed** before starting a run:

- **0.5x:** Slows down playback (helpful for debugging complex workflows).
- **1x (Normal):** Uses the recorded delays without scaling.
- **2x, 5x, 10x:** Speeds up execution proportionately.
- **100x Turbo:** Scales recorded delays to 1/100 of their original length.
- **Max Speed (Instant):** Skips recorded delays; processing and input delivery still take time.
- **Custom:** Set any multiplier from `0.01x` to `1000x`.

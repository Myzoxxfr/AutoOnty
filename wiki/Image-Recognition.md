# Image Recognition

AutoOnty can look for an image on your screen and act when it appears. Use it in **macros** to wait, click or run another macro, or in **autoclick** to click while selected images are visible.

## Choose an image

Choose **Take screenshot** to open Windows snipping, or **Choose file** to import a BMP, PNG or JPG. Capture a small, distinctive button or icon and keep the target visible when searching. AutoOnty removes a new matching Snipping Tool PNG from Screenshots only after saving its own copy.

The **Image** page lets you adjust matching:

- **Sensitivity:** Higher values require a closer match. Start with Normal and adjust if needed.
- **Test:** Checks for the image without clicking.
- **Also try different image sizes:** Helps with resized targets, but takes longer.

Use the same display/app scale as your screenshot where possible. Minimize AutoOnty if it covers the target. Matching and click timing can slow down with large images or many saved images.

## Use with macros

For an image-only watcher:

1. Open **Menu -> Macro settings -> Image recognition macro...** and choose an image.
2. Under **When found**, choose **Click once**, **Auto click**, **Run a saved macro**, or **Wait only**. For clicking, choose the image center, cursor or a fixed position.
3. Set **Waiting & repeat**, then choose **Start watching** or **Save macro...**.

You can act once, repeat after the image disappears, keep acting while it is visible, or act once at each location. Repeating stays on the image step; choose Once to continue after the first match. Set time/count limits to keep a watcher from running indefinitely. Choose whether a match or timeout should continue, pause or stop the macro.

For **Auto click**, set a click interval in milliseconds, a click count and **Autoclick run time** in seconds. A click count of `0` means until the image disappears; run time `0` adds no time limit. **Run a saved macro** stores a copy, so editing the original file later does not update the image action.

To add an image step to a recording, use **Add image** in the macro editor. It defaults to **Wait only**. Double-click an image row or choose **Image settings...** to edit it.

**Find image timeout** is in seconds; `0` keeps searching. Image steps added to recordings default to 3 seconds; new image-only watchers and autoclick images default to `0`. Waiting resets after a match. See [Playback & Editing](Playback-and-Editing.md) for editor and pause controls.

## Use with autoclick

The autoclick image list is separate from your recorded or loaded macro and supports up to 64 images.

1. Open **Menu -> Autoclick settings -> Image recognition...** and add images.
2. Set each image's click destination, interval and limits, then save the list.
3. Choose **Use images when autoclicking**, select one or more images and confirm.
4. Start with the Autoclick toolbar button or hotkey.

Each image can click once, a specific number of times, or keep clicking while visible. Clicking stops when the image disappears or its limit is reached. Limits reset after the image disappears and reappears. **Click interval** uses milliseconds; **Autoclick run time** uses seconds, with `0` unlimited. Each image also has a **Find image timeout** in seconds. When one expires, the other images continue; autoclick stops when all selected images have timed out.

Your normal button, single/double click, countdown, hold mode, jitter, window targets and total repeat limit still apply. Double sends two physical clicks per click action. Each image uses its own destination and interval.

Recognition starts disabled each launch; saved images and your selection are remembered. To change the selection, disable and enable **Use images when autoclicking** again. See [Autoclick](Autoclick.md) for the general controls.

## Stop and save

The panic key (default **Pause**) stops either workflow. Macro playback also has a Stop button. For autoclick, press the hotkey again in toggle mode or release it in hold mode; the toolbar button always toggles.

Keep the **ImageDetections** folder with macros that use images when moving or sharing them. Autoclick images use `Documents\AutoOnty\ImageDetections`. See [storage and backups](Settings.md#storage-and-backups) for backing up settings and images together.

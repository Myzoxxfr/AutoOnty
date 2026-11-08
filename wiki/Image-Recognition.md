# Image Recognition

Macros and autoclicking use one shared library of up to 64 images. Each image has separate macro and autoclick settings.

## Add and manage images

Open **Image recognition...** under **Autoclick settings**, or **Image recognition macro...** under **Macro settings**. Both show the same images, with Add, Image settings and Delete controls.

Choose a screenshot or import a BMP, PNG or JPG. Images save to `Documents\AutoOnty\ImageDetections`. Canceling removes newly captured, unused images. A matching new Snipping Tool PNG is removed from Screenshots after AutoOnty's copy is saved.

Use a small, distinctive image and keep the target visible. **Test** searches without clicking. Sensitivity controls how close a match must be; **Also try different image sizes** helps with resized targets but takes longer.

Delete removes the entry from both views and deletes its BMP when you choose **Done**. Cancel keeps it. If a saved macro uses the image, AutoOnty asks before deleting the file because that macro would no longer load.

## Use with macros

1. Open **Macro settings -> Image recognition macro...** and select one image.
2. Use **Image settings** to choose clicking, running a saved macro, or **Continue macro** after detection.
3. Choose **Save macro...**. The new macro opens in the editor, where you can add more image steps and other actions. **Start watching** runs it directly.

**Done**, the blue button, saves library changes without creating a macro.

Choosing **Run a saved macro** opens the picker in `Documents\AutoOnty`. Saving creates a new macro with an image wait followed by copies of the chosen macro's actions. The image step becomes **Continue macro**; edits and deletion of the original file do not affect the copy.

Use **Add image** in the macro editor to pick another image from the library. Double-click an image step to change its settings.

**Matches before continuing** counts detections handled by an image step, not clicks. `3` finishes that step after three matches and continues the macro; `0` is unlimited. Use **Again after it disappears** to count separate appearances. **Once** handles one match regardless of the limit.

## Use with autoclick

1. Open **Autoclick settings -> Image recognition...**.
2. Add or edit images, tick the ones to use, then choose **Done**.
3. Start autoclicking with its toolbar button or hotkey.

The menu entry shows a tick when image mode is active. Untick every image and choose **Done** to return to ordinary autoclicking. Recognition starts disabled each launch; your images and selection are remembered.

Each image has **Number of clicks** under **When found**. `0` keeps clicking while visible. **Click interval** uses milliseconds; **Autoclick run time** uses seconds, with `0` unlimited.

Under **Waiting & repeat**, **Find image timeout** uses seconds; `0` keeps searching. **Match limit** caps completed appearances; `0` is unlimited. **Time between actions** adds a delay in milliseconds before handling the next appearance.

**After a match** applies when clicking finishes: **Keep looking**, **Pause autoclick**, or **Stop autoclick**. Resume with the pause hotkey or **Pause / resume** in the menu to search again. Clicking limits reset after disappearance and reappearance; the match limit applies across the run.

Normal button, single/double click, countdown, hold mode, jitter, window targets and total repeat limits still apply. See [Autoclick](Autoclick.md).

## Storage and stopping

Existing saved autoclick images and BMPs in `ImageDetections` are imported into the shared library. Existing macro files keep their own saved settings.

Images deleted manually from the folder are removed from the library when you next open it.

The panic key stops either workflow. Keep `ImageDetections` with macros when moving or sharing them. See [storage and backups](Settings.md#storage-and-backups).

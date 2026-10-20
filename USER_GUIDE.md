# Morse Bird — User Guide

**A playful Morse-code keyer for Pebble Time 2 (Emery)**

Morse Bird turns your Pebble watch into a CW (continuous-wave) sending key. The animated drinking bird taps a telegraph sounder while you send Morse using the watch's physical buttons. Decoded characters appear on screen as you transmit.

> **Current build name:** The existing project still identifies itself as **AC1RV Key** in `package.json`. **Morse Bird** is the chosen app name; this guide describes the current `feature/animate-bird-import` implementation rather than promising a separately released app.

## Quick start

1. Open Morse Bird / AC1RV Key on your Pebble Time 2.
2. In the default **Paddle** mode, tap **UP** to send a **dit** (`.`) or **DOWN** to send a **dah** (`-`).
3. Hold either button for repeated dits or dahs; hold both to alternate elements (the existing simple squeeze-keyer behavior).
4. Release the buttons and pause between letters. Recognized characters appear in the text area.
5. Press **SELECT once** to open Settings. Press **SELECT twice** to clear the displayed text.

The small white semicircle at the right edge is a visual reminder for the center SELECT/menu button.

## Controls

| Watch button | Paddle mode | Straight Key mode |
| --- | --- | --- |
| UP | Send dits (default; can swap) | Sends manually timed Morse **only** if Top is selected |
| DOWN | Send dahs (default; can swap) | Sends manually timed Morse **only** if Bottom is selected |
| SELECT, single click | Open Settings | Open Settings |
| SELECT, double click | Clear decoded text and pending undecoded code | Same |
| BACK | Standard Pebble navigation | Standard Pebble navigation |

The app waits briefly (up to **300 ms**) after a center-button tap to distinguish a single click from a double click.

### Paddle mode

Paddle mode is selected by default. Dits and dahs use the selected sending speed to set element duration: at 20 WPM a dit lasts 60 ms and a dah 180 ms. Between automatically generated elements, the app inserts one dit-length gap. Holding both paddles alternates elements; this is a **simple squeeze keyer**, not a user-selectable iambic A/B implementation.

To swap the buttons, go to **Settings → Paddles**. In the paddle-choice dialog, select UP for normal mapping or DOWN for reversed mapping.

### Straight Key mode

Go to **Settings → Keying mode** to switch from Paddle to **Straight key**. Then use **Settings → Straight key** to select the **Top** or **Bottom** button.

- Press the selected button to begin a tone (and vibration, if enabled).
- Hold it for as long as you want the Morse element to last.
- Release it to end the element.
- The other sending button has no function in Straight Key mode.
- SELECT still opens Settings and clears on double-click.

**Straight Key decoding** automatically estimates your sending rhythm within approximately **10–20 WPM**, independently of the paddle speed setting. The adaptive timing is used only to interpret press durations as dits or dahs and to detect letter and word gaps. Your physical key press directly controls when the tone starts and stops; the app does not force your sending speed into that range. The decoder starts with a 15 WPM estimate and adjusts as you send.

## Reading the display

The main screen uses Pebble-blue with the bird and sounder above **three rows of white decoded text**. There is no header or filled sidebar.

- **New characters appear on the top row.**
- The current row fills from left to right. When the *next* character would exceed the available width, the finished top row moves down, the previous middle row moves to the bottom, and the next character starts again at the left of the top row.
- On further overflow, the oldest bottom row disappears.
- The app measures character width, so the number of letters per line is not fixed.
- A recognized question mark is shown as **`?`** (Morse `..--..`). An unrecognized or overlong Morse sequence appears as **`*`**.
- Double-clicking SELECT clears **all three rows** and any Morse element sequence waiting to be decoded.

Characters are decoded after a pause of approximately three dit durations; a longer pause of approximately seven dit durations separates words. The text display is a live sending aid, **not** an outgoing text-message interface.

## Settings reference

Press SELECT once, then move with UP/DOWN and press SELECT to choose an option.

| Menu setting | Range or behavior | Initial value |
| --- | --- | --- |
| **Speed** | 5–50 WPM; UP/DOWN in the speed editor | 20 WPM |
| **Keying mode** | Paddle / Straight key | Paddle |
| **Straight key** | Top button / Bottom button | Top |
| **Tone frequency** | 200–1500 Hz, in 50 Hz steps | 600 Hz |
| **Tone** | On / Off | On |
| **Vibration** | On / Off; battery warning on enabling | Off |
| **Paddles** | Normal or reversed dit/dah assignment | UP = dit, DOWN = dah |

Settings are stored on the watch and normally persist between app launches.

### Auditioning the tone

Open **Settings → Tone frequency**. The watch **automatically plays a sustained sample** of the selected square-wave sidetone. Press UP or DOWN to adjust it in 50 Hz steps and hear each selection. Leave the editor with BACK to stop the sample. The preview runs even when sending-tone playback is disabled; it does not change the Tone On/Off setting.

The sidetone is a deliberately buzzy **square wave**. The implementation currently uses a fixed output-volume parameter rather than a separate volume menu.

### Tone and vibration combinations

You can configure **both tone and vibration**, **tone only**, **vibration only**, or **neither**. Turning vibration **on** displays a warning because frequent haptic feedback consumes additional battery. Select to acknowledge and enable, or press BACK to cancel. Turning vibration off does not require confirmation.

The vibration setting affects sending feedback. Whether continuous haptic playback feels exactly like a physical key depends on the watch hardware.

## Bird animation

The drinking bird animates toward the telegraph sounder when you send. On release, it returns to its poised position; after roughly **1.2 seconds** of inactivity, it returns home. Sending again during the return reverses the motion toward the key.

The animation is visual feedback. The keying engine determines Morse timing independently; the motion is **not** a precise frame-for-frame depiction of each 24–240 ms element.

## Troubleshooting

**One SELECT tap feels slightly delayed.** The delay allows time to detect the double-click clear gesture.

**A letter appears as `*`.** The element sequence was not recognized. Try spacing the dits/dahs more distinctly or adjust Speed to match your natural straight-key timing.

**No sound.** Check **Settings → Tone**, then check the watch's audio behavior. The tone-frequency editor can preview a tone even when the sending Tone setting is off.

**No vibration.** Check **Settings → Vibration** and explicitly enable it after acknowledging the battery warning.

**A sending button does nothing.** In Straight Key mode, only the chosen Top or Bottom button sends; the other is intentionally inactive. Check **Settings → Straight key**.

**The tone sounds clipped or crackly at higher speed.** Try a lower WPM and test on physical hardware. At 50 WPM a dit is only **24 ms**, which can expose speaker/audio timing limitations. The current implementation has not been independently hardware-validated for all settings.

**The screen isn't behaving like a normal text editor.** It isn't one: incoming decoded letters always enter the *top* row, pushing older rows downward on overflow.

## Availability and development status

This guide documents the source currently on GitHub, not a fully tested app-store release. The project targets **Pebble Time 2 / Emery**. Audio, button timing, animation, and vibration should be checked on a real watch as well as in the emulator.

- Repository: [Darachnid/ac1rv-key](https://github.com/Darachnid/ac1rv-key)
- Development branch: [feature/animate-bird-import](https://github.com/Darachnid/ac1rv-key/tree/feature/animate-bird-import)
- Original source code: `src/c/main.c`

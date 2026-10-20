# AC1RV Key

A Pebble Time 2 (Emery) CW keyer with a Pebble-native settings UI and a custom sending screen.

## Current behavior

- 20 WPM default
- UP = dit and DOWN = dah by default
- Hold paddles for repeated elements
- Squeeze both paddles to alternate
- SELECT opens the on-watch settings menu
- Settings:
  - Speed (5–35 WPM), with animated value changes
  - Paddle order (choose whether UP sends dit or dah)
- 600 Hz square-wave sidetone
- Haptic keying feedback
- Live Morse decoding on the sending screen
- Settings persist on-watch

The keying engine is adapted from the send/keyer implementation in
`Darachnid/pebble-cw-messenger`, but this is a separate standalone app.

## UI

The main screen follows the AC1RV Key mockup:

- Vivid Cerulean main field
- No status bar or title/header on the sending screen
- Transparent right-side controls with white icons on the blue background:
  - top: dit/dah symbol for UP
  - middle: clipped semicircle indicating SELECT opens the menu
  - bottom: dit/dah symbol for DOWN
- large decoded CW text, displayed in three rows with newest text on top; when the current row fills, rows shift downward and the next character starts at column 1
- unknown/undecodable Morse sequences display as `*`
- only the white SELECT semicircle remains at the right edge; dit/dah rail icons are omitted and text uses the full screen width

The bird and sounder are baked as a single 14-frame PDC vector sequence
(`resources/animations/frames.pdc`) at the Emery 200x228 coordinate system.
The animation is driven independently from Morse timing:

- 001 is the home pose
- 009 is the poised pose
- 014 is the pressed pose
- a key press targets 014
- key release targets 009
- after 1.2 seconds of inactivity, the bird returns to 001
- if keying resumes during the return, the animation immediately reverses toward
  the pressed pose

The Morse timers remain authoritative; the animation never delays a dit or dah.

## Build

```sh
pebble build
pebble install --cloudpebble
```

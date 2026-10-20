# AC1RV Key

A Pebble Time 2 (Emery) CW keyer with a Pebble-native settings UI and a custom sending screen.

## Current behavior

- 20 WPM default
- UP = dit and DOWN = dah by default
- Hold paddles for repeated elements
- Squeeze both paddles to alternate
- SELECT opens the on-watch settings menu
- Settings:
  - Speed (5–40 WPM), with animated value changes
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
- Pebble `StatusBarLayer` with white foreground
- Custom right-side rail:
  - top: dit/dah symbol for UP
  - middle: clipped semicircle indicating SELECT opens the menu
  - bottom: dit/dah symbol for DOWN
- large decoded CW text

The bird/sounder art area is intentionally kept separate from the keyer and UI
logic so the final PDC sequence can be dropped in without changing Morse timing.

## Build

```sh
pebble build
pebble install --cloudpebble
```

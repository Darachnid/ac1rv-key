# Import the Adobe Animate bird animation

This branch keeps the existing AC1RV CW paddle implementation and its 14-frame
animation state machine. It does **not** replace Morse timing, paddle handling,
settings, or the user interface.

The Adobe Animate export contains 73 full-resolution SVG frames. The existing
app presently uses 14 individual PNG poses, so this importer samples key poses
from the export and updates only the 14 PNG resources.

## On macOS

1. Download or clone this branch:
   `git clone -b feature/animate-bird-import https://github.com/Darachnid/ac1rv-key.git`
2. Install Inkscape if it isn't installed.
3. Copy `keyer-bird-frames.zip` from Adobe Animate to your Mac, without unzipping.
4. In Terminal:
   ```sh
   cd ac1rv-key
   python3 scripts/import_animate_frames.py ~/Downloads/keyer-bird-frames.zip
   pebble build
   pebble install --emulator emery
   ```
5. Inspect the animation in the emulator before committing any generated PNGs.
6. To commit generated assets to this branch:
   ```sh
   git add resources/images/keyer/
   git commit -m "Import new Adobe Animate bird poses"
   git push origin feature/animate-bird-import
   ```

The index mapping is deliberately editable inside the script:
home is Adobe frame 1, the poised/contact pose is frame 21, and the pressed
pose is frame 24. The current C animation uses its 14-frame indices to animate
between home, ready, and pressed states. The later overshoot frames from the
full Adobe animation are *not* yet incorporated; that requires intentionally
extending the state machine. Keep the full original SVG ZIP as your animation
master.

Note: Pebble does not play APNG as an animation resource. These individual
PNGs are converted and bundled using the existing Pebble SDK resource pipeline.

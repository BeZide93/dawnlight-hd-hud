# v2.5.0 local validation

The original HUD changes are on `hud-issues-and-updater`, based on v2.4.4.
The ROM-import repository is independent and local-only; its existing files and
staged changes were preserved. No ROM-import source was used for these changes.

Completed:

- macOS Apple Silicon native module and package build.
- Android ARM64 native module and package build.
- 26 native regression tests, including updater installation/failure handling,
  JSON/version parsing, SHA-256 known vectors, map input policy, font resources,
  and separate iOS/tvOS notice-only updater fixtures.
- 34 Python source/resource and native-event fixture checks, including actual
  Steam Deck/DualSense texture selectors with distinct native texture fixtures.
- Package ZIP verification and Alegreya font specimen inspection.

Windows compilation remains unverified: the existing local Zig/MSVC toolchain
references a missing `/private/tmp/twilight-hud-xwin` SDK. No Windows package
from this change has been produced. iOS/tvOS notice-only logic was tested with
platform fixtures; no signed Apple mobile app has been produced.

Before publishing or closing the corresponding issues, verify in Dusklight:

1. Show Renado's Letter to Telma from R, including with X empty or holding a
   different item. Repeat from X and Y, and test another trade-item NPC.
2. Turn D-Pad Shortcuts off, restart, and close dungeon/overworld maps using Left.
   Confirm native minimap hide behavior and warp preference handling.
3. Enable shortcuts and test combined Up cycling and the separate layout,
   Midna on each direction, touch controls, and a second mod using released keys.
4. Inspect Alegreya dialogue with accents and long lines in supported languages.
5. Inspect the HUD Sizing right-panel bullets, numbered Steam Deck shoulders,
   and DualSense BOTW/Flipped BOTW prompts in gameplay, menus, and item assignment.
   Recheck Save, Collection, and prompts in GameCube/Wii U/Dusklight scaling modes.
   Issue #52 already has reporter confirmation of the v2.4.4 fix.
6. Test GitHub checks on a connected host. Desktop/Android installation requires
   a newer release, confirmation, a verified package, and a restart. iOS/tvOS
   must offer a notice only and require a newly bundled and signed app.

The local test packages under `Builds/v2.5.0-local` each contain one platform's
native module. They are not complete Universal releases. No release has been
published and no issues have been closed by this work.

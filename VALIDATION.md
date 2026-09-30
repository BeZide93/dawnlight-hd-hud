# Release validation

Run the native tests with `ctest --test-dir build --output-on-failure`.
The platform build workflow also checks resources, native hook behavior,
architecture, package metadata, texture formats, and archive integrity.

Gameplay checks:

- Present Renado's Letter from R with X empty or holding another item; compare
  with X/Y presentation and another trade-item NPC.
- Test maps with D-Pad Shortcuts on and off, including combined Up cycling,
  Midna bindings, touch input, and minimap state after warping.
- Check fonts, sizing controls, and controller prompts in gameplay and menus.
- Check Save and Collection in each Dusklight scaling mode.
- Test update checks and installation from an older version. iOS and tvOS must
  offer notices only; their native updates require a signed app build.
- Follow the Dawnlight checks in [COMPATIBILITY.md](COMPATIBILITY.md).

Native hook tests use mocked game state. They do not replace the quest-specific
in-game checks above.

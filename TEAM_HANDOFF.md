# Something Moved — Team Handoff

## Integration branch

Active development is on `anas-gameplay`. Do not merge to `master` until Debug and Release builds, both self-tests, and a live victory/failure playthrough have been verified. Never force-push or commit `bin`, `obj`, `.vs`, the raw asset library, or machine-specific settings.

## Current playable system

- Ten-round observation campaign with a guided teddy tutorial and authored wardrobe finale.
- Eight aligned full-frame bedroom anomalies defined by the `RoomObject` catalog in `GameModel::InitializeObjects`.
- Tiered early/middle/late selection, shorter late-game timers, no immediate repeats, scoring, three-mistake failure, timeout feedback, restart, and results statistics.
- Atmospheric rain, dust, warm/cool light, lightning, blackout flicker, vignette, hover and answer glows.
- Mouse selection, persistent keyboard selection, pause/resume, mute, focus gating, and aspect-ratio-aware input mapping.
- Debug and Release `--self-test` validation, including 10,000 deterministic anomaly selections.

## Ownership boundaries

- Gameplay/state changes: `GameModel.*`
- Presentation only: `GameRenderer.*`
- Input mapping: `InputController.*`
- Sound events: `AudioController.*`
- Cross-system lifecycle: `GameApp.*`

Keep the anomaly catalog authoritative: name, description, tier, property, full-frame variant index, and hit region must remain in sync. The renderer derives the room variant from that catalog instead of maintaining a second anomaly list.

## Required verification before integration

1. Build `Debug|x64` and run `SomethingMoved.exe --self-test`.
2. Build `Release|x64` and run the same test.
3. Launch Release and verify intro, tutorial, pause, mouse, keyboard selection, mute, one correct answer, one wrong answer, and timeout.
4. Complete a ten-round victory and a three-mistake game over.
5. Confirm a clean checkout includes all nine `Media\Textures\BedroomRoom` images.

Raw source packs remain outside the runtime commit because their licenses prohibit standalone redistribution. Use only cleared, selected derivatives and update `ASSET_CREDITS.md` when adding assets.

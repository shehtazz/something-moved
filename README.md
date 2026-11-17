# Something Moved

Something Moved is a cinematic 2D psychological-horror observation game built in C++ with LARC and DirectX 12. Study the bedroom, survive the blackout, and identify the single anomaly before the room changes again.

> Team project for CSCE 4210 at the University of North Texas, Fall 2026. Built by Anas Harun, Isabella Pantoja, and Shehtaz Mahboob.

## My contributions

- Designed and implemented the hint system: press **H** during a Find round to dim one randomly chosen wrong object. Two hints per round, use them or lose them, 100 points each, and a hint never dims the changed object.
- Wired the H key through `InputController`, handled it in `GameApp`, and rendered the dimmed objects plus the hints-remaining HUD counter in `GameRenderer`.
- Added self-test coverage for the hint rules (reset each round, exhaustion after two uses, never dims the changed object) in `RunModelSelfTests`.

## Play

- **Enter / Space:** confirm, or choose the keyboard-highlighted object
- **Mouse:** hover and click an object
- **Arrow keys / WASD:** menus and keyboard object selection
- **H:** use a hint, dims one wrong object (2 per round, costs 100 points)
- **P:** pause or resume
- **M:** mute or unmute
- **Escape:** resume from pause, return to the menu, or exit from the menu
- **R:** restart after victory or game over
- **F1:** debug information
- **F2 / F3 (Debug builds):** advance phase / submit the correct anomaly

The campaign contains a guided first round, progressively shorter observation and search phases, eight authored anomalies, a three-mistake limit, and an authored final round.

## Build

Requirements:

- Visual Studio 2026 with Desktop C++ support and Windows 10/11 SDK
- `LARCENGINE_DIR` pointing to the LARC engine root
- `DIRECTXTK12_DIR` pointing to DirectXTK12

Open `SomethingMoved.sln`, select `Debug|x64` or `Release|x64`, and build. The executable is written to `bin\x64\<Configuration>\SomethingMoved.exe` and finds the repository `Media` directory when launched directly.

Run automated model/content validation with:

```text
SomethingMoved.exe --self-test
```

## Architecture

- `GameModel`: deterministic campaign state, anomaly catalog, scoring, timing, and hit testing
- `GameRenderer`: LARC sprite presentation, room variants, atmosphere, UI, and feedback
- `InputController`: focus-safe mouse/keyboard input and logical 1280x720 coordinate scaling
- `AudioController`: rain ambience, blackout and answer cues, and mute
- `GameApp`: lifecycle and integration

Runtime assets and licenses are summarized in `ASSET_CREDITS.md`. Generated `bin`, `obj`, and Visual Studio workspace files are intentionally excluded from Git.

# pong-afterhours

Minimal Pong showing how to use the [afterhours](https://github.com/gabeochoa/afterhours) ECS: entities, components, systems, and the input + window-manager plugins. First to 5 wins.

## Build

Afterhours is a git submodule — clone with `--recurse-submodules`, or run `git submodule update --init` after cloning.

Requires Homebrew's raylib — the build finds `raylib.h`/`raymath.h` via pkg-config:

```sh
brew install raylib pkg-config
make build   # builds pong.exe without launching it
make run     # builds and plays it
make e2e     # runs the afterhours e2e scripts in tests/e2e, headless
make e2e-window  # the same scripts in a visible window
make clean
```

The e2e scripts drive the game with injected keyboard input (no Accessibility
permission needed) and assert on real game state: pause, paddle movement,
launch, wall bounce, paddle hit, scoring, win at 5, restart, and frame rate
over several seconds of play. Under e2e the game steps a fixed 1/60 dt and the
RNG is seeded per script, so runs repeat exactly. `screenshot <name>` steps
write PNGs to `tests/e2e/screenshots/`.

The menu/score text uses [Atkinson Hyperlegible](https://brailleinstitute.org/freefont) ([resources/fonts/Atkinson-OFL.txt](/Users/gabeochoa/p/pong-afterhours/resources/fonts/Atkinson-OFL.txt)), SIL OFL 1.1. Gamepad mappings live in [resources/gamecontrollerdb.txt](/Users/gabeochoa/p/pong-afterhours/resources/gamecontrollerdb.txt), loaded by afterhours' input plugin.

## Controls

| | P1 | P2 |
|---|---|---|
| Paddle | Up/Down arrows or gamepad 1 | W/S or gamepad 2 |
| Serve | Space (either player) | |
| Pause | P or Esc | |
| Restart | R | |


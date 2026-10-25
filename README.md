# pong-afterhours

Minimal Pong showing how to use the [afterhours](https://github.com/gabeochoa/afterhours) ECS: entities, components, systems, and the input + window-manager plugins. First to 5 wins.

## Build

Requires Homebrew's raylib — the vendored tree has no `raylib.h`/`raymath.h`, so the build finds them via pkg-config:

```sh
brew install raylib pkg-config
make build   # builds pong.exe without launching it
make run     # builds and plays it
make clean
```

## Controls

| | P1 | P2 |
|---|---|---|
| Paddle | Up/Down arrows or gamepad 1 | W/S or gamepad 2 |
| Serve | Space (either player) | |
| Pause | P or Esc | |
| Restart | R | |


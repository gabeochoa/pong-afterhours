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

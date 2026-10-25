# afterhours gaps

Library gaps found while making pong-afterhours the smallest example of how
to use afterhours. Pong is now a 415-line `main.cpp` plus 147 lines of e2e
commands (it was 562 + 426), with no vendored code of its own.

Bar for adding any item: would three or more games use it. Do not absorb
game rules (collision, scoring, win condition, pause semantics, serve
rules). Those are the game itself, not scaffolding, and absorbing them would
leave the example with nothing to teach.

## Done

- **App runner**: `graphics::RunConfig` + `graphics::run()` (callbacks:
  `init` / `frame` / `cleanup`). `canvas_width/height` makes run() own a
  fixed-size render texture and present it letterboxed.
- **E2E setup helper**: `testing::E2EApp::attach(cfg, argc, argv, ...)`
  wraps run() with the e2e loop, fixed dt, `--headless` and exit code.
- **E2E command lambdas**: `testing::register_command(sm, name, fn)`.
- **Crash handler**: `crash::install_handler()` already existed; it replaced
  pong's vendored backward-cpp.
- **Text layout**: `draw_text_centered`, `draw_text_right_aligned`,
  `draw_text_with_shadow`.
- **Synth beep**: `synth::beep(freq, seconds)`.
- **Font with fallback**: no library change needed. Load the font, then
  `if (!font_is_usable(f)) f = get_default_font();`.
- **Letterbox present**: `draw_render_texture(rt, dest, tint)` handles the
  per-backend flip. The canvas uses it.
- **`hold_key` / `release_key`**: e2e builtins. The builtin `key` only taps.
- **`PendingE2ECommand::expect(ok, mismatch)`**: consumes, or retries with the mismatch.
- **`rect::bounds(a, b)`**: rect union, used for pong's swept collision rect.
- **FPS assert**: `perf_commands` `expect_fps_above` already existed.
- **Headless**: two modes, no runtime branching. `DisplayMode::Headless` on
  raylib is a hidden window (real GL, clock, sizes). `AFTER_HOURS_USE_NONE` is
  a compile-time platform backend with no window or GPU. Its input reads the
  e2e injector, so a headless build plays scripts. `input::gamepad::` and
  `graphics::FLAG_WINDOW_HIGHDPI` let game code stay backend-neutral.
- **Repeatable e2e**: `E2EApp::set_seed(s)` seeds `RandomEngine` per script,
  opt-in (runs are random by default). Pong opts in.
- **Rect overlap / centre**: `rect::overlaps` / `rect::center` already existed.

## Open

### Input mapping DSL

Builder/table for action bindings. Low value: pong's `get_mapping()` is
already a 15-line brace-initialised table.

### Standard `Transform` / `HasVelocity`

Shared components. Must keep `prev` (swept collision uses it). Wait for a
second consumer.

### Input-collector base

An `InputSystem<...>` base that injects the collector. Saves about 8 lines
across 2 systems. Revisit when a third game wants it.

## Explicitly not gaps

Collision response, scoring, win condition, pause semantics, serve rules.
If the library absorbs them, pong stops being an example of how to use
afterhours and becomes configuration for pong-in-a-box.

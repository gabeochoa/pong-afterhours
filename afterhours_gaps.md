# afterhours gaps

Library gaps found while making pong-afterhours the smallest example of how
to use afterhours. Pong is a 562-line `main.cpp` plus 426 lines of e2e
support, of which ~200 lines are actual game logic — the rest is
scaffolding afterhours could absorb.

Bar for adding any item: would three or more games use it. Do not absorb
game rules (collision, scoring, win condition, pause semantics, serve
rules) — those are the game, not scaffolding, and absorbing them would
leave the example with nothing to teach.

## Gaps

### 1. App runner

One call owning window init, dt clamp, letterbox present, pause gating,
teardown. Replaces most of pong `main()` (~144 lines -> ~45).
Open design decision: callbacks/lambdas vs subclass-and-override. That
choice sets the shape of every future game.

### 2. E2E setup helper

One call for parse args, handler registration order, runner tick + merge,
screenshot capture, results/exit code. Replaces pong's
`src/e2e_integration.h` (~137 -> ~25). Pieces already exist
(`parse_e2e_args`, `configure_runner`, `register_all_handlers`); the
per-game wiring does not.

### 3. Text layout: `draw_text_centered`, `draw_text_right_aligned`, `draw_text_with_shadow`

Replaces magic offsets in pong `RenderScore` / `RenderFPS` / menu
(`GAME_W / 2 - 140`, `-190`, ...). Would have fixed pong's FPS-inset and
menu-shadow screenshot issues with one-line calls.

### 4. Input mapping DSL

Builder/table for action bindings. Replaces pong `get_mapping()`
(~57 -> ~20). Keep the syntax readable; it must stay a plain mapping
underneath.

### 5. Synth beep: `synth::beep(freq, seconds)` with attack/release envelope

Replaces pong `make_beep` (~28 -> ~4).

### 6. Font with fallback

A font handle that degrades to the default font automatically. Replaces
pong `draw_game_text` + `font_ready` global (~13 -> ~3).

### 7. Standard `Transform` / `HasVelocity`

Shared components in a common header. Replaces pong's structs
(~24 -> ~5). Must keep `prev` (pong's swept collision uses it), or pong
keeps its own `Transform`.

### 8. Letterbox helper: `graphics::draw_letterboxed(target)`

Replaces pong's scale / `draw_texture_pro` math (~15 -> 1). Subsumed by
the App runner if that exists; filed separately so it can land first.

### 9. Input-collector base

An `InputSystem<...>` base that injects the collector. Replaces repeated
`inpc` + `once()` in pong `ImpulseBall` / `ImpulsePaddle` (~12 -> ~4).

### 10. E2E command lambdas: `testing::command("name", handler)`

Replaces struct-per-command boilerplate in pong `src/e2e_commands.h`
(~289 -> ~200). Structs may be clearer in an example; weigh readability
before adding.

## Suggested order

1. Text layout helpers — smallest, zero design risk.
2. E2E setup helper — biggest per-line win; every game reimplements it.
3. Input mapping DSL — densest boilerplate left in game code.
4. App runner — biggest overall win, but needs the callbacks-vs-subclass
   decision first.

## Explicitly not gaps

Collision, scoring, win condition, pause semantics, serve rules. If the
library absorbs them, pong stops being an example of how to use afterhours
and becomes configuration for pong-in-a-box.

# todo

Audit of 2026-10-24: 68 bugs found by source inspection (no runtime/gamepad testing).
1 critical, 25 high, 28 medium, 14 low. Each item is one distinct root cause;
cross-slice duplicates were merged. Suggested order if fixing in batches:
#1 (UB), #2-6 (core gameplay/collision), #7-12 (input), #13-17 (loop/window/dt), #18-26 (build).

Vendor bugs live in `vendor/afterhours`, shared with other `~/p/` projects — fix
upstream there if the fix belongs to the library, not just in this repo.

## Critical

- [x] **`gen_first_enforce` out-of-bounds read on empty query (UB).** (`vendor/afterhours/src/entity_query.h:186`) — logs an error when `values` is empty, then unconditionally `return values[0];`. Fix: throw / return an empty optional instead of indexing an empty vector; callers such as `MoveAndBounce::once` must handle the empty case.

## High

- [x] **No scoring, winner, or game end on out-of-bounds.** (`src/main.cpp:191`) — out-of-bounds just recentres the ball and zeroes velocity, with `// TODO add to score`; no score state exists anywhere. Fix: award a point by exit side before reset; store/display scores; add a win condition.

- [x] **`Collide` flips both velocity axes on any overlap.** (`src/main.cpp:301`) — `hasVel.vel = hasVel.vel * -1.f` reverses x and y even for a head-on paddle-face hit, wrongly changing the vertical angle. Fix: flip only the penetration/collision-normal axis (x for a paddle face).

- [x] **`Collide` lacks an approaching check.** (`src/main.cpp:298`) — flips on any overlap with no check on velocity direction, so it flips the ball even when moving away while still overlapping, sending it back into the paddle. Fix: flip only when the entity is moving toward the collider (velocity along the collision normal).

- [x] **`Collide` has no separation or cooldown.** (`src/main.cpp:301`) — no push-out, no per-pair cooldown/timestamp; a ball still overlapping next frame flips again, causing jitter/double-hits and FPS-dependent outcomes. Fix: push the ball out along the normal on collision and/or add a short per-pair cooldown.

- [x] **`Collide` flips paddle velocity too.** (`src/main.cpp:301`) — `for_each_with` runs for every `Transform + HasVelocity` entity, including paddles (which have `HasVelocity`), and flips its own velocity on overlap — a ball–paddle hit reverses the paddle as well as the ball. Fix: only flip the ball (e.g. skip entities with `PlayerID`), and reverse x only.

- [x] **Shared keyboard mapping for both paddles.** (`src/main.cpp:67`) — `get_mapping()` gives both players `KEY_UP`/`KEY_DOWN`; there are no W/S (or other) keys for P2, so one keyboard cannot drive two paddles independently. Fix: add per-player keyboard bindings (e.g. arrows for P1, W/S for P2).

- [x] **P2 keyboard input is dead via the id filter.** (`src/main.cpp:216`) — `ImpulsePaddle` drops actions with `actions_done.id != playerID.id`, but keyboard actions are recorded with `id = 0`; P2 (`id = 1`) therefore never sees keyboard input even if distinct keys were mapped. Fix: share keyboard input with both paddles (keyed by binding, not gamepad id) or tag keyboard actions per-player.

- [x] **Keyboard input duplicated per gamepad id.** (`vendor/afterhours/src/plugins/input_system.h:310`) — the `do { ... } while (i <= max_gamepad_id_available)` loop re-emits keyboard results for every id, while `check_*` ignores `id` for `KeyCode` inputs. Fix: poll keyboard once outside the per-gamepad loop and tag it `Keyboard` / `id = -1`.

- [x] **`fetch_max_gampad_id` returns -1 when all gamepads present.** (`vendor/afterhours/src/plugins/input_system.h:218`) — `result = -1` is set only on the first unavailable id (`break`); if all 8 ids are available the loop ends with `result` still -1, and a gapped layout (pad 0 missing, pad 1 present) also truncates to -1. Fix: scan all ids, track the highest available id, and handle the all-present case (result = `MAX_GAMEPAD_ID - 1`).

- [x] **Ball launch uses held input, not edge-triggered input.** (`src/main.cpp:150`) — `ImpulseBall` reads `inpc.inputs()` (key/button down), so holding SPACE through a reset while `vel == 0` auto-relaunches the ball. Fix: use `inputs_pressed()` for the one-shot `Launch` action.

- [x] **Gamepad DB path is CWD-relative.** (`src/main.cpp:333`) — `std::ifstream ifs("gamecontrollerdb.txt")` resolves against the working directory; the makefile never copies/installs the DB, so launching from another directory silently skips mappings (file exists at repo root). Fix: resolve the DB path relative to the executable/bundle, or install it alongside the binary.

- [x] **Update runs inside `BeginDrawing`.** (`src/main.cpp:393`) — `systems.run()` is called between `BeginDrawing()` and `EndDrawing()`; `system.h:216` shows `run()` = `tick_all()` + `render_all()`, so game logic executes inside the draw block. Fix: split tick before `BeginDrawing`, and run only render systems inside the draw block.

- [x] **Resolution fetched once, never re-fetched.** (`vendor/afterhours/src/plugins/window_manager.h:89`) — `CollectCurrentResolution` clears `should_refetch` after the first fetch and nothing ever sets it true again (no resize/fullscreen trigger). Fix: set `should_refetch = true` on resize/fullscreen events and re-fetch then.

- [x] **HiDPI mismatch: framebuffer pixels vs logical coordinates.** (`vendor/afterhours/src/plugins/window_manager.h:58`) — `fetch_current_resolution()` uses `GetRenderWidth/Height` (framebuffer px), but draw/logic use logical coordinates — on Retina the bounds are 2x, so the ball exits the visible view. Fix: use `GetScreenWidth/Height` (logical size) for game bounds.

- [x] **Hardcoded spawn geometry ignores the resolution singleton.** (`src/main.cpp:316`) — `make_paddle` uses x = 150/1100 and y = 720/2, `make_ball` uses 1280/2, 720/2, while bounds come from `ProvidesCurrentResolution`; a different window size misplaces spawns. Fix: derive spawn positions from the resolution singleton / named constants.

- [x] **Unclamped variable dt enables tunneling.** (`src/main.cpp:394`) — `systems.run(raylib::GetFrameTime())` passes raw frame time; `MoveAndBounce` steps `p += vel * 500 * dt`, so a hitch can teleport the ball through a paddle/wall. Fix: clamp dt (e.g. `<= 1/30` s) or use a fixed timestep.

- [x] **No `clean` recipe despite `.PHONY: clean`.** (`makefile:42`) — `.PHONY` lists `clean` but no `clean:` target exists, so `make clean` is a no-op and `pong.exe`/outputs are never removed. Fix: add a `clean` recipe deleting `$(OUTPUT_EXE)`, `output/`, and `*.dSYM`.

- [x] **`DEPENDS` uses undefined `SOURCES`.** (`makefile:32`) — `$(patsubst %.cpp,%.d,$(SOURCES))` with `SOURCES` never set yields an empty `DEPENDS`, so `-include` loads nothing; the defined variable is `SRC_FILES`. Fix: use `$(SRC_FILES)` in the `patsubst`.

- [x] **`all` builds one TU and runs the game.** (`makefile:44`) — `all` compiles only `src/main.cpp` and `&& ./pong.exe`, ignoring `SRC_FILES`/`OBJ_FILES` and blocking CI/headless builds. Fix: split into a `build` target (compile all `SRC_FILES`) and a separate `run` target.

- [x] **`brew` runs at parse time on every invocation.** (`makefile:5`) — `export PKG_CONFIG_PATH := $(shell brew --prefix)/...` executes brew for every make invocation (including `clean`/`--help`), and a brew failure leaves a leading `:` (CWD in the search path). Fix: guard brew failure / lazily compute the path only for build targets.

- [x] **Hardcoded Homebrew/macOS link line.** (`makefile:10`) — `$(shell brew --prefix raylib)/lib/libraylib.a` plus `-framework OpenGL/Cocoa/IOKit/...` breaks Linux/Windows/Intel-brew setups. Fix: use `pkg-config --libs raylib` with conditional macOS-only frameworks.

- [x] **Dead submodule declaration.** (`.gitmodules:1`) — declares `vendor/afterhours` as a submodule via `git@github.com:gabeochoa/afterhours.git`, yet the files are present as plain tracked files in the tree. Fix: remove the declaration or restore a pinned real submodule.

- [x] **GLFW include path case mismatch.** (`src/rl.h:26`) — `#include <GLFW/glfw3.h>` needs a `vendor/GLFW/` directory, but the vendored directory is lowercase `vendor/glfw/` (verified by listing); only Homebrew's GLFW satisfies the include. Fix: align the include path / directory case (e.g. `<glfw/glfw3.h>`).

- [x] **Vendored raylib headers missing.** (`vendor/RaylibOpOverloads.h:3`) — includes `<raylib.h>`/`<raymath.h>`, but `vendor/` contains no `raylib.h`/`raymath.h` (only `afterhours/`, `backward/`, `fmt/`, `glfw/`, `magic_enum/`, `RaylibOpOverloads.h`), so the build works only via Homebrew pkg-config. Fix: vendor the raylib headers or document the Homebrew requirement.

- [x] **Y-bounce double-advances after the flip.** (`src/main.cpp:187`) — position is stepped once at line 179, then after `vel.y *= -1` the identical `p += vel * sz * dt` runs twice (lines 187–188) — 3 steps in one frame vs 1 normally, net `p0 - v*step` instead of a mirrored overshoot. Fix: step once after the flip and clamp `p.y` to `[0, H - h]`.

## Medium

- [x] **Out-of-bounds test uses top-left x only.** (`src/main.cpp:191`) — `p.x < -10 || p.x > map_width + 10` tests the top-left corner, so the ball resets with ~20px still visible on the left and only after fully off-screen on the right (ball size is 30). Fix: test `p.x + size.x < 0` / `p.x > width`.

- [x] **Serve is fully deterministic (always up-left).** (`src/main.cpp:150`) — every launch sets `vel = (-0.5, -1.0)`; P1 always receives first with the same angle. Fix: randomise serve direction/angle per serve, or serve toward the conceding player.

- [x] **Single-step discrete collision can tunnel.** (`src/main.cpp:179`) — one `vel * sz * dt` step plus AABB overlap; a large dt moves the ball more than the 30px paddle thickness, skipping the paddle entirely. Fix: substep the movement or use swept AABB.

- [x] **Paddle wall hit discards the move instead of clamping.** (`src/main.cpp:184`) — if a paddle (`PlayerID`) is out of y-bounds, `for_each_with` returns without updating/clamping, so it stops short of the edge and never recovers if spawned outside. Fix: clamp `p.y` to `[0, height - size.y]` and apply the update.

- [x] **Ball launch ignores player id.** (`src/main.cpp:140`) — `ImpulseBall` never checks `actions_done.id`, so any player's `Launch` fires the ball — unlike `ImpulsePaddle`, which filters by player id. Fix: filter `Launch` to the serving player (or document any-player launch as intended).

- [x] **Missing gamepad DB fails silently.** (`src/main.cpp:334`) — `!is_open()` only prints to `cout` and returns; `main` (line 350) ignores the failure, so the game continues with default mappings and no visible warning. Fix: propagate the error (stderr + nonzero exit, or an on-screen warning).

- [x] **Paddle keeps stale velocity if the input collector is missing.** (`src/main.cpp:208`) — `!has_value()` returns before the `else` branch that zeroes `vel.y`, so a missing collector leaves the previous frame's velocity in place. Fix: zero velocity before the early return.

- [x] **Simultaneous up+down picks up instead of cancelling.** (`src/main.cpp:230`) — `if (up) ... else if (down)` makes up win when both are held (with merged keyboard+gamepad inputs). Fix: set velocity to 0 when both up and down are pressed.

- [x] **Analog magnitude discarded.** (`vendor/afterhours/src/plugins/input_system.h:315`) — `visit_axis` returns the actual magnitude, but the collector stores `amount_pressed = 1.f`, so analog deflection never scales paddle speed. Fix: store the actual `amount` and scale paddle speed by it.

- [x] **`PossibleInputCollector::inputs()` unchecked.** (`vendor/afterhours/src/plugins/input_system.h:188`) — `inputs()`, `inputs_pressed()`, and `since_last_input()` call `data.value()` directly, throwing `bad_optional_access` if the collector is missing; callers must pre-check `has_value()`. Fix: check inside the accessors (return an empty vector / 0) or document the pre-check contract.

- [x] **Input freshness relies on implicit registration order.** (`src/main.cpp:373`) — input systems are registered before `ImpulseBall`/`ImpulsePaddle` with no guarantee; reordering would make them read stale inputs. Fix: document/enforce the ordering (or make the input tick an explicit first phase).

- [x] **`ProvidesTargetFPS` is dead.** (`src/main.cpp:348`) — `SetTargetFPS(200)` is hardcoded, the singleton `ProvidesTargetFPS` (added at line 356 with 200) is never read, and no consumer calls `SetTargetFPS` from it. Fix: drive FPS from the singleton, or delete the singleton.

- [x] **No `SetConfigFlags` before `InitWindow`.** (`src/main.cpp:347`) — no `FLAG_WINDOW_RESIZABLE`/`VSYNC`/`HIGHDPI` is set, leaving a fixed window with tearing. Fix: call `SetConfigFlags` before `InitWindow`.

- [x] **`RenderEntities` ignores `HasColor`.** (`src/main.cpp:309`) — `System<Transform>` draws every rectangle `RAYWHITE`; `HasColor` (line 117) is never added to any entity or read. Fix: include `HasColor` in the render system (per-entity colour), or delete it.

- [x] **No letterboxing/scaling for non-1280x720 windows.** (`src/main.cpp:392`) — the game draws raw-pixel coordinates directly to the backbuffer, with no `RenderTexture`/viewport; on other sizes the playfield stretches/misplaces. Fix: render to a 1280x720 texture and letterboxed-blit it to the window.

- [ ] **`Collide` is O(n^2) per frame.** (`src/main.cpp:294`) — a full `EQ().gen()` scan runs inside `for_each_with` for every `Transform + HasVelocity` entity. Fix: collect colliders once in `once()` (or once per tick) and reuse them.

- [ ] **Input collector re-queried per paddle per frame.** (`src/main.cpp:206`) — `get_input_collector()` runs a full `EntityQuery` scan inside each `for_each_with` call. Fix: fetch the collector once per tick and pass/cache it.

- [ ] **`EQ::whereInRange`/`orderByDist` dead and unsafe.** (`src/main.cpp:255`) — never called, and both call `entity.get<Transform>()` with no `has<Transform>` guard (`.at` throws on a missing component). Fix: delete them, or guard with `has<Transform>()`.

- [ ] **`HasVelocity.vel` lacks an initializer.** (`src/main.cpp:114`) — `vec2 vel;` has no ctor/init; zeroing relies only on `make_unique` value-init, while direct/stack construction leaves it indeterminate. Fix: `vec2 vel{};`.

- [ ] **`Limit` off-by-one allows n+1.** (`vendor/afterhours/src/entity_query.h:50`) — `if (amount_taken > amount) return false; amount_taken++;` passes 2 results for `take(1)`. Fix: use `>=`.

- [ ] **`CXX :=` blocks environment override.** (`makefile:40`) — `CXX := clang++` overrides environment `CXX` (only command-line assignment beats it), breaking GCC/Linux setups. Fix: `CXX ?= clang++`.

- [ ] **Draft `-std=c++2c`.** (`makefile:18`) — `FLAGS` uses `-std=c++2c`, an experimental pre-C++26 dialect, not a stable portable standard. Fix: use `-std=c++23`.

- [ ] **Empty compilation database.** (`compile_commands.json:1`) — root file is `[]`; the makefile never generates it, so clangd has no translation unit for `src/main.cpp`. Fix: generate it from the build, or stop tracking the empty file.

- [ ] **Inverted macro guard overwrites the user value.** (`vendor/afterhours/ah.h:14`) — `#if defined(AFTER_HOURS_MAX_COMPONENTS)` then `#define AFTER_HOURS_MAX_COMPONENTS 128` overwrites a user-provided value, and never defines it when unset. Fix: flip to `#ifndef`.

- [ ] **Header not self-contained.** (`vendor/afterhours/src/plugins/input_system.h:4`) — includes only `<map>`/`<variant>` yet uses `std::string`, `std::to_string`, and `std::pair`, relying on transitive includes. Fix: add `<string>` and `<utility>` (and any other directly used headers).

- [ ] **No pause/menu/restart state.** (`src/main.cpp:392`) — the main loop only checks `WindowShouldClose`; there is no pause, menu, game-over, or restart state machine. Fix: add a game-state machine and a pause key.

- [ ] **`InitWindow` failure unchecked.** (`src/main.cpp:347`) — no check that window/context creation succeeded before systems and the loop run. Fix: check `IsWindowReady()` (or equivalent) and abort with an error.

- [ ] **`MoveAndBounce::once` missing `override`.** (`src/main.cpp:161`) — `void once(float)` has no `override` (sibling systems use it); clang `-Winconsistent-missing-override` warns. Fix: add `override`.

## Low

- [ ] **`Collide` ignores paddle velocity and hit offset.** (`src/main.cpp:301`) — ball angle/speed are unchanged by a moving paddle or by where on the paddle the ball hits. Fix: add rebound angle from the hit offset plus paddle-velocity transfer.

- [ ] **One speed scalar for ball and paddles.** (`src/main.cpp:177`) — `sz = 500` gives paddles 500 px/s but the ball only 250 px/s horizontally (`vel.x = -0.5`), so paddles outrun the ball horizontally. Fix: separate ball and paddle speed constants.

- [ ] **HUD draw order lets entities overdraw FPS text.** (`src/main.cpp:386`) — `RenderFPS` is registered before `RenderEntities`, so the ball/paddles can overdraw the FPS readout. Fix: register `RenderFPS` last.

- [ ] **Wrong window title.** (`src/main.cpp:347`) — `InitWindow` title is `'wm-afterhours'`, a window-manager leftover for a pong game. Fix: rename to `'pong-afterhours'`.

- [ ] **ESC quits instantly.** (`src/main.cpp:392`) — no `SetExitKey`; raylib's default ESC sets `WindowShouldClose`, with no pause/confirm. Fix: `SetExitKey(KEY_NULL)` and handle ESC as pause.

- [x] **`Transform::update` takes a non-const reference.** (`src/main.cpp:100`) — `void update(vec2 &v)` never modifies `v`, blocking rvalue/temporary arguments. Fix: `const vec2 &v`.

- [ ] **Exact float equality for the launch check.** (`src/main.cpp:150`) — `vel.vel.x == 0.f && vel.vel.y == 0.f` uses exact comparison; fractional or NaN velocity never launches. Fix: use an epsilon comparison or an explicit launched flag.

- [ ] **`NOFLAGS` dead; `-Werror` never applied.** (`makefile:45`) — `NOFLAGS` defines `-Werror`, but `all` compiles with only `$(FLAGS)`. Fix: add `$(NOFLAGS)` to the compile command.

- [ ] **`button_size` unused.** (`src/main.cpp:54`) — `const vec2 button_size = {100, 50};` is never referenced (grep shows only the definition). Fix: remove it.

- [ ] **`focus_rect` unused.** (`src/main.cpp:106`) — `Transform::focus_rect()` is defined but never called (grep shows only the definition). Fix: remove it, or use it for a focus/debug outline.

- [ ] **Dead negation.** (`.gitignore:20`) — `!/lib*.dylib` has no preceding `*.dylib` ignore, so it matches nothing and does nothing. Fix: add a `*.dylib` ignore or drop the negation.

- [ ] **No tests or test target.** (`makefile:44`) — the repo has no test files and the makefile defines only `all`/`clean`, so regressions have no gate. Fix: add tests plus a `test` target.

- [ ] **No gamepad-connection feedback.** (`src/main.cpp:387`) — the `RenderConnectedGamepads` render system is commented out, so players cannot tell whether a second pad is connected. Fix: re-enable it or show connection status another way.

- [ ] **No sound or audio init.** (`src/main.cpp:343`) — no `InitAudioDevice`/`PlaySound` anywhere, so hits and scores are silent. Fix: add audio feedback for hits and scores.

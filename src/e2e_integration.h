// Wires the afterhours e2e harness into pong: parses --e2e args, registers
// the builtin handlers plus pong's commands, advances the script runner each
// frame, and writes `screenshot <name>` PNGs from the game's render texture.
//
// Included after the game types in main.cpp, since the pong commands inspect
// them. When --e2e is absent, setup() returns nullptr and the game runs
// exactly as before.
#pragma once

// Upstream's animation header shadows a field; silence that one warning for
// the upstream includes only, not for pong's own code.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "afterhours/src/plugins/e2e_testing/e2e_testing.h"
#include "afterhours/src/plugins/e2e_testing/harness.h"
#include "afterhours/src/plugins/e2e_testing/platform_test_input.h"
#include "afterhours/src/plugins/e2e_testing/runner.h"
#include "afterhours/src/plugins/e2e_testing/test_input.h"
#pragma GCC diagnostic pop
#include "e2e_commands.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace pong_e2e {
using namespace afterhours;
using namespace afterhours::testing;

struct State {
  std::unique_ptr<E2ERunner> runner;
  // The e2e handlers live in their own SystemManager, ticked every frame
  // even while the game is paused: the gameplay systems only tick when
  // unpaused, so handlers registered there would never consume a command
  // from the menu.
  SystemManager *handler_systems = nullptr;
  std::string screenshot_dir = "tests/e2e/screenshots";
  std::string pending_screenshot;
  graphics::RenderTextureType *target = nullptr;
};

inline bool requested(int argc, char *argv[]) {
  return should_run_e2e(argc, argv);
}

inline std::unique_ptr<State> setup(SystemManager &handler_systems, int argc,
                                    char *argv[]) {
  if (!requested(argc, argv))
    return nullptr;

  E2EArgs args = parse_e2e_args(argc, argv);
  // Route every input reader through the injector: the input plugin only
  // consults injected input while test_mode is on (the main loop's
  // platform_input checks the injector first either way).
  platform_input::set_test_mode(true);

  auto state = std::make_unique<State>();
  state->handler_systems = &handler_systems;
  state->screenshot_dir = args.screenshot_dir.empty()
                              ? "tests/e2e/screenshots"
                              : args.screenshot_dir;
  E2ERunner &runner = *(state->runner = std::make_unique<E2ERunner>());
  configure_runner(runner, args);

  // Documented registration order: builtins, [screenshot, reset], custom,
  // unknown, cleanup.
  register_builtin_handlers(handler_systems);
  handler_systems.register_update_system(
      std::make_unique<HandleScreenshotCommand>(
          [state = state.get()](const std::string &name) {
            state->pending_screenshot = name;
          }));
  handler_systems.register_update_system(
      std::make_unique<HandleResetTestStateCommand>(
          [state = state.get()] { state->pending_screenshot.clear(); }));
  register_pong_commands(handler_systems);
  register_unknown_handler(handler_systems);
  register_cleanup(handler_systems);

  if (!args.script_dir.empty())
    runner.load_scripts_from_directory(args.script_dir);
  else if (!args.script_path.empty())
    runner.load_script(args.script_path);
  if (!runner.has_commands()) {
    log_error("e2e: no scripts found (dir '{}', file '{}')", args.script_dir,
              args.script_path);
    return nullptr;
  }
  return state;
}

// Runs before the gameplay systems each frame: resets per-frame input,
// dispatches the next script command, merges the command entity so the
// handlers see it this frame, and runs the handlers (before gameplay, so
// injected input and test hooks land in the same frame's input/gameplay
// systems).
inline void tick(State &state, float dt) {
  test_input::reset_frame();
  state.runner->tick(dt);
  EntityHelper::merge_entity_arrays();
  if (state.handler_systems)
    state.handler_systems->tick_all(EntityHelper::get_entities_for_mod(), dt);
}

// After end_drawing: writes a pending `screenshot <name>` from the game's
// own render texture.
inline void post_render(State &state) {
  if (state.pending_screenshot.empty() || !state.target)
    return;
  std::string name = std::move(state.pending_screenshot);
  state.pending_screenshot.clear();
  std::filesystem::create_directories(state.screenshot_dir);
  std::filesystem::path path =
      std::filesystem::path(state.screenshot_dir) / (name + ".png");
  auto png = capture_render_texture_png(*state.target);
  if (!png) {
    log_warn("e2e: screenshot '{}' failed to capture", name);
    return;
  }
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(png->bytes.data()),
            (std::streamsize)png->bytes.size());
  if (!out)
    log_warn("e2e: screenshot '{}' failed to write {}", name, path.string());
}

inline bool is_finished(State &state) { return state.runner->is_finished(); }

// Prints the per-script results. Exit 0 only if the runner finished and
// every script passed.
inline int finish(State &state) {
  state.runner->print_results();
  return state.runner->is_finished() && !state.runner->has_failed() ? 0 : 1;
}

} // namespace pong_e2e

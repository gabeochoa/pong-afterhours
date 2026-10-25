#include "backward/backward.hpp"

namespace backward {
backward::SignalHandling sh;
} // namespace backward
#include "std_include.h"

#define AFTER_HOURS_USE_RAYLIB
// Routes the input plugin's key/button polling through the e2e injector
// (without it, the collector only sees the real keyboard and injected input
// never reaches gameplay).
#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "afterhours/ah.h"
#include "afterhours/src/drawing_helpers.h"
#include "afterhours/src/font_helper.h"
#include "afterhours/src/graphics.h"
#include "afterhours/src/plugins/audio_helpers.h"
#include "afterhours/src/plugins/input_system.h"
#include "afterhours/src/plugins/window_manager.h"
#include "afterhours/src/random_engine.h"

using namespace afterhours;

constexpr int WIN_SCORE = 5;
constexpr float BALL_SPEED = 500.f;
constexpr float PADDLE_SPEED = 450.f;
constexpr int GAME_W = 1280;
constexpr int GAME_H = 720;
constexpr char FONT_PATH[] =
    "resources/fonts/AtkinsonHyperlegible-Regular.ttf";

struct GameState : BaseComponent {
  int left = 0;
  int right = 0;
  bool game_over = false;
  bool paused = true;
};

// The custom font, or raylib's default font when it fails to load.
static Font game_font{};

static SoundType hit_sound{};
static SoundType score_sound{};

struct RenderScore : System<GameState> {
  virtual void for_each_with(const Entity &, const GameState &state,
                             const float) const override {
    draw_text_centered(
        game_font,
        (std::to_string(state.left) + " : " + std::to_string(state.right))
            .c_str(),
        GAME_W / 2, 10, 30, raylib::RAYWHITE);
    if (state.game_over) {
      draw_text_centered(
          game_font,
          ((state.left > state.right ? "P1" : "P2") +
           std::string(" WINS - R to play again"))
              .c_str(),
          GAME_W / 2, GAME_H / 2 - 40, 30, raylib::RAYWHITE);
    }
  }
};

struct RenderFPS : System<window_manager::ProvidesCurrentResolution> {
  virtual void for_each_with(
      const Entity &,
      const window_manager::ProvidesCurrentResolution &, const float) const
      override {
    draw_text_right_aligned(
        game_font,
        (std::to_string((int)graphics::get_fps()) + " FPS").c_str(),
        GAME_W - 10, 0, 20, raylib::GREEN);
  }
};

enum class InputAction {
  None,
  P1Up,
  P1Down,
  P2Up,
  P2Down,
  Launch,
};

auto get_mapping() {
  std::map<int, input::ValidInputs> mapping;
  auto key = [](InputAction a) { return static_cast<int>(a); };
  mapping[key(InputAction::P1Up)] = {
      raylib::KEY_UP,
      input::GamepadAxisWithDir{
          .axis = raylib::GAMEPAD_AXIS_LEFT_Y,
          .dir = -1,
      },
      raylib::GAMEPAD_BUTTON_LEFT_FACE_UP,
  };

  mapping[key(InputAction::P1Down)] = {
      raylib::KEY_DOWN,
      input::GamepadAxisWithDir{
          .axis = raylib::GAMEPAD_AXIS_LEFT_Y,
          .dir = 1,
      },
      raylib::GAMEPAD_BUTTON_LEFT_FACE_DOWN,
  };

  mapping[key(InputAction::P2Up)] = {
      raylib::KEY_W,
      input::GamepadAxisWithDir{
          .axis = raylib::GAMEPAD_AXIS_LEFT_Y,
          .dir = -1,
      },
      raylib::GAMEPAD_BUTTON_LEFT_FACE_UP,
  };

  mapping[key(InputAction::P2Down)] = {
      raylib::KEY_S,
      input::GamepadAxisWithDir{
          .axis = raylib::GAMEPAD_AXIS_LEFT_Y,
          .dir = 1,
      },
      raylib::GAMEPAD_BUTTON_LEFT_FACE_DOWN,
  };

  mapping[key(InputAction::Launch)] = {
      raylib::KEY_SPACE,
      raylib::GAMEPAD_BUTTON_RIGHT_FACE_UP,
      raylib::GAMEPAD_BUTTON_RIGHT_FACE_DOWN,
      raylib::GAMEPAD_BUTTON_RIGHT_FACE_LEFT,
      raylib::GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,
  };
  return mapping;
}

struct Transform : BaseComponent {
  vec2 position;
  vec2 size;
  vec2 prev;

  vec2 pos() const { return position; }
  void update(const vec2 &v) {
    prev = position;
    position = v;
  }
  Transform(vec2 pos, vec2 sz) : position(pos), size(sz), prev(pos) {}
  raylib::Rectangle rect() const {
    return raylib::Rectangle{position.x, position.y, size.x, size.y};
  }
};

struct HasVelocity : BaseComponent {
  vec2 vel{};
};

struct PlayerID : BaseComponent {
  input::GamepadID id;
  PlayerID(input::GamepadID i) : id(i) {}
};

struct ImpulseBall : System<HasVelocity> {
  input::PossibleInputCollector inpc;

  void once(const float) override {
    inpc = input::get_input_collector();
  }

  // Any player may serve: there is no serve ownership in pong, so Launch
  // deliberately ignores the action's player id (unlike ImpulsePaddle).
  virtual void for_each_with(Entity &entity, HasVelocity &vel,
                             const float) override {
    if (entity.has<PlayerID>())
      return;

    if (!inpc.has_value()) {
      return;
    }
    bool space = false;

    for (auto &actions_done : inpc.inputs_pressed()) {
      if (actions_done.action == static_cast<int>(InputAction::Launch))
        space = true;
    }

    if (space && std::abs(vel.vel.x) < 1e-6f &&
        std::abs(vel.vel.y) < 1e-6f) {
      float angle =
          (float)RandomEngine::get().get_int(-35, 35) * (3.14159265f / 180.f);
      float dir = RandomEngine::get().get_bool() ? 1.f : -1.f;
      vel.vel.x = dir * cosf(angle);
      vel.vel.y = sinf(angle);
    }
  }
};

struct MoveAndBounce : System<Transform, HasVelocity> {
  virtual void for_each_with(Entity &entity, Transform &transform,
                             HasVelocity &vel, const float dt) override {
    float speed = entity.has<PlayerID>() ? PADDLE_SPEED : BALL_SPEED;
    vec2 p = transform.pos();
    p.x += vel.vel.x * speed * dt;
    p.y += vel.vel.y * speed * dt;

    if (entity.has<PlayerID>()) {
      p.y = std::clamp(p.y, 0.f, (float)GAME_H - transform.size.y);
      transform.update(p);
      return;
    }

    if (p.y < 0 || p.y + transform.size.y > GAME_H) {
      vel.vel.y *= -1;
      if (p.y < 0)
        p.y = -p.y;
      else
        p.y = 2 * (GAME_H - transform.size.y) - p.y;
    }

    if (p.x + transform.size.x < 0 || p.x > GAME_W) {
      auto scored = EntityQuery().whereHasComponent<GameState>().gen_first();
      afterhours::PlaySound(score_sound);
      if (scored.valid()) {
        GameState &state = scored.asE().get<GameState>();
        if (p.x + transform.size.x < 0)
          state.right++;
        else
          state.left++;
        state.game_over =
            state.left >= WIN_SCORE || state.right >= WIN_SCORE;
      }
      p = vec2{GAME_W / 2.f, GAME_H / 2.f};
      vel.vel = {0, 0};
    }

    transform.update(p);
  }
};

struct ImpulsePaddle : System<HasVelocity, PlayerID> {
  input::PossibleInputCollector inpc;

  void once(const float) override {
    inpc = input::get_input_collector();
  }

  virtual void for_each_with(Entity &, HasVelocity &vel, PlayerID &playerID,
                             const float) override {

    if (!inpc.has_value()) {
      vel.vel.y = 0.f;
      return;
    }

    bool up = false;
    bool down = false;

    int up_action = static_cast<int>(playerID.id == 0 ? InputAction::P1Up
                                                      : InputAction::P2Up);
    int down_action = static_cast<int>(playerID.id == 0
                                           ? InputAction::P1Down
                                           : InputAction::P2Down);

    for (auto &actions_done : inpc.inputs()) {
      // Keyboard actions arrive once per gamepad id (id 0 with no pads), so
      // the paddle accepts any keyboard input for its own action; gamepad
      // input must match its pad id. Per-player actions keep the shared
      // keyboard input from cross-talking.
      if (actions_done.id != playerID.id &&
          actions_done.medium != input::DeviceMedium::Keyboard)
        continue;
      if (actions_done.action == up_action)
        up = true;
      else if (actions_done.action == down_action)
        down = true;
    }

    if (up && down)
      vel.vel.y = 0.f;
    else if (up)
      vel.vel.y = -1.f;
    else if (down)
      vel.vel.y = 1.f;
    else
      vel.vel.y = 0.f;
  }
};

struct Collide : System<Transform, HasVelocity> {
  RefEntities colliders;

  void once(const float) override {
    colliders = EntityQuery().whereHasComponent<Transform>().gen();
  }

  virtual void for_each_with(Entity &entity, Transform &transform,
                             HasVelocity &hasVel, const float) override {
    if (entity.has<PlayerID>())
      return;

    const auto swept = rect::bounds(
        transform.rect(), raylib::Rectangle{transform.prev.x, transform.prev.y,
                                            transform.size.x, transform.size.y});

    for (auto &ref : colliders) {
      Entity &other_e = ref.get();
      if (other_e.id == entity.id)
        continue;
      const Transform &other = other_e.get<Transform>();
      if (!rect::overlaps(swept, other.rect()))
        continue;

      const auto c = rect::center(transform.rect());
      const auto oc = rect::center(other.rect());
      const vec2 d{c.x - oc.x, c.y - oc.y};
      bool approaching =
          (hasVel.vel.x > 0 && d.x < 0) || (hasVel.vel.x < 0 && d.x > 0);
      if (!approaching)
        continue;

      hasVel.vel.x *= -1.f;

      float offset = d.y / (other.size.y / 2.f);
      float paddle_vy = 0.f;
      if (other_e.has<HasVelocity>())
        paddle_vy = other_e.get<HasVelocity>().vel.y;
      hasVel.vel.y =
          std::clamp(offset + paddle_vy * 0.2f, -1.f, 1.f);
      float dir = hasVel.vel.x > 0 ? 1.f : -1.f;
      hasVel.vel.x =
          dir * sqrtf(std::max(0.f, 1.f - hasVel.vel.y * hasVel.vel.y));

      vec2 p = transform.pos();
      p.x = hasVel.vel.x > 0 ? other.pos().x + other.size.x
                             : other.pos().x - transform.size.x;
      transform.update(p);
      afterhours::PlaySound(hit_sound);
      break;
    }
  }
};

struct RenderEntities : System<Transform, HasColor> {
  virtual void for_each_with(const Entity &, const Transform &transform,
                             const HasColor &color,
                             const float) const override {
    draw_rectangle(transform.rect(), color.color());
  }
};

void make_paddle(input::GamepadID id) {
  auto &entity = EntityHelper::createEntity();

  vec2 position = {.x = id == 0 ? 150.f : (float)GAME_W - 180.f,
                     .y = (float)GAME_H / 2.f};

  entity.addComponent<PlayerID>(id);
  entity.addComponent<Transform>(position, vec2{30.f, 150.f});
  entity.addComponent<HasVelocity>();
  entity.addComponent<HasColor>(raylib::RAYWHITE);
}

Entity &make_ball() {
  auto &entity = EntityHelper::createEntity();

  vec2 position = {.x = (float)GAME_W / 2.f, .y = (float)GAME_H / 2.f};

  entity.addComponent<Transform>(position, vec2{30.f, 30.f});
  entity.addComponent<HasVelocity>();
  entity.addComponent<HasColor>(raylib::YELLOW);
  return entity;
}

// Pong's e2e commands; included here because they inspect the game types.
#include "e2e_commands.h"

int main(int argc, char *argv[]) {
  SystemManager systems;
  std::unique_ptr<testing::E2EApp> e2e;
  graphics::RenderTextureType target{};
  GameState *state = nullptr;
  Entity *ball = nullptr;
  int exit_code = 0;

  graphics::RunConfig cfg;
  cfg.width = GAME_W;
  cfg.height = GAME_H;
  cfg.title = "pong-afterhours";
  cfg.target_fps = 200;
  cfg.flags = graphics::FLAG_WINDOW_RESIZABLE | graphics::FLAG_VSYNC_HINT |
              raylib::FLAG_WINDOW_HIGHDPI;

  cfg.init = [&] {
    if (!graphics::is_window_ready()) {
      std::cerr << "error: failed to create window\n";
      exit_code = 1;
      graphics::request_quit();
      return;
    }
    graphics::set_exit_key(raylib::KEY_NULL);
    afterhours::InitAudioDevice();
    hit_sound = synth::beep(330.f, 0.09f);
    score_sound = synth::beep(660.f, 0.16f);

    game_font = load_font_from_file(FONT_PATH, 32);
    if (!font_is_usable(game_font))
      game_font = get_default_font();

    auto &singletons = EntityHelper::createEntity();
    input::add_singleton_components(singletons, get_mapping());
    window_manager::add_singleton_components(singletons, cfg.target_fps);
    state = &singletons.addComponent<GameState>();

    make_paddle(0);
    make_paddle(1);
    ball = &make_ball();

    // Entities are created in a temp array; tick() normally merges them, but
    // the game starts paused (no tick yet) while render already needs them.
    EntityHelper::merge_entity_arrays();

    e2e = testing::E2EApp::create(argc, argv, pong_e2e::register_pong_commands,
                                  "tests/e2e/screenshots");
    if (testing::should_run_e2e(argc, argv) && !e2e) {
      exit_code = 1;
      graphics::request_quit();
      return;
    }

    window_manager::enforce_singletons(systems);
    input::enforce_singletons(systems);

    // External plugins first: gameplay systems read this frame's input.
    input::register_update_systems(systems);
    window_manager::register_update_systems(systems);

    systems.register_update_system(std::make_unique<ImpulseBall>());
    systems.register_update_system(std::make_unique<ImpulsePaddle>());
    systems.register_update_system(std::make_unique<MoveAndBounce>());
    systems.register_update_system(std::make_unique<Collide>());

    systems.register_render_system(
        [](float) { graphics::clear_background(raylib::DARKGRAY); });
    systems.register_render_system(std::make_unique<RenderScore>());
    systems.register_render_system(std::make_unique<RenderEntities>());
    systems.register_render_system(
        std::make_unique<input::RenderConnectedGamepads>());
    systems.register_render_system(std::make_unique<RenderFPS>());

    target = load_render_texture(GAME_W, GAME_H);
  };

  cfg.frame = [&] {
    float dt = std::min(graphics::get_frame_time(), 1.f / 30.f);
    if (e2e)
      e2e->tick(dt);
    // platform_input reads injected input under --e2e, the keyboard otherwise.
    if (testing::platform_input::is_key_pressed(raylib::KEY_R)) {
      state->left = state->right = 0;
      state->game_over = false;
      state->paused = false;
      ball->get<Transform>().update(
          vec2{(float)GAME_W / 2.f, (float)GAME_H / 2.f});
      ball->get<HasVelocity>().vel = {0, 0};
    }
    if ((testing::platform_input::is_key_pressed(raylib::KEY_P) ||
         testing::platform_input::is_key_pressed(raylib::KEY_ESCAPE)) &&
        !state->game_over)
      state->paused = !state->paused;
    if (!state->paused && !state->game_over)
      systems.tick_all(EntityHelper::get_entities_for_mod(), dt);
    begin_texture_mode(target);
    systems.render_all(dt);
    if (state->paused) {
      // Centred + shadow: measure, then offset by half the width.
      auto draw_menu_text = [](const char *text) {
        const float w = measure_text(game_font, text, 30, 1.f).x;
        draw_text_with_shadow(game_font, text, GAME_W / 2.f - w * 0.5f,
                              GAME_H / 2.f - 20, 30, raylib::RAYWHITE,
                              raylib::BLACK);
      };
      if (state->left == 0 && state->right == 0)
        draw_menu_text("PONG - P to play");
      else
        draw_menu_text("PAUSED - P to resume");
    }
    end_texture_mode();
    graphics::begin_drawing();
    graphics::clear_background(raylib::BLACK);
    // Letterbox the fixed-size game texture into the resizable window.
    draw_render_texture(target,
                        window_manager::content_viewport(
                            graphics::get_screen_width(),
                            graphics::get_screen_height())
                            .dest,
                        raylib::WHITE);
    graphics::end_drawing();
    if (e2e) {
      e2e->post_render(target);
      if (e2e->finished())
        graphics::request_quit();
    }
  };

  cfg.cleanup = [&] {
    if (e2e)
      exit_code = e2e->finish();
    unload_render_texture(target);
    afterhours::UnloadSound(hit_sound);
    afterhours::UnloadSound(score_sound);
    afterhours::CloseAudioDevice();
  };

  graphics::run(cfg);
  return exit_code;
}

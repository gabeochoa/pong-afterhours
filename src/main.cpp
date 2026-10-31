#include <algorithm>
#include <cmath>
#include <map>
#include <string>

// Routes input polling through the e2e injector; without it injected input
// never reaches gameplay.
#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "afterhours/ah.h"
#include "afterhours/src/crash_handler.h"
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
// Paddle hits cap the rebound angle at 60° from horizontal: |vy| <= sin(60°)
// leaves |vx| = sqrt(1 - vy²) >= 0.5, so the ball can never go purely
// vertical (where vx = 0) and get stuck bouncing between the walls.
constexpr float kMaxBallVelY = 0.866f;
constexpr float PADDLE_SPEED = 450.f;
constexpr int GAME_W = 1280;
constexpr int GAME_H = 720;
constexpr Vector2Type kCenter{GAME_W / 2.f, GAME_H / 2.f};
constexpr char FONT_PATH[] =
    "resources/fonts/AtkinsonHyperlegible-Regular.ttf";

// raylib's palette, spelled out so the headless (none) build has it too.
constexpr ColorType kRayWhite{245, 245, 245, 255};
constexpr ColorType kDarkGray{80, 80, 80, 255};
constexpr ColorType kYellow{253, 249, 0, 255};
constexpr ColorType kGreen{0, 228, 48, 255};
constexpr ColorType kBlack{0, 0, 0, 255};

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
        GAME_W / 2, 10, 30, kRayWhite);
    if (state.game_over) {
      draw_text_centered(
          game_font,
          ((state.left > state.right ? "P1" : "P2") +
           std::string(" WINS - R to play again"))
              .c_str(),
          GAME_W / 2, GAME_H / 2 - 40, 30, kRayWhite);
    }
  }
};

enum class InputAction { P1Up, P1Down, P2Up, P2Down, Launch };

std::map<int, input::ValidInputs> get_mapping() {
  using pad = input::gamepad;
  const auto stick = [](int dir) {
    return input::GamepadAxisWithDir{.axis = pad::AXIS_LEFT_Y, .dir = dir};
  };
  const auto id = [](InputAction a) { return static_cast<int>(a); };
  return {
      {id(InputAction::P1Up), {keys::UP, stick(-1), pad::LEFT_FACE_UP}},
      {id(InputAction::P1Down), {keys::DOWN, stick(1), pad::LEFT_FACE_DOWN}},
      {id(InputAction::P2Up), {keys::W, stick(-1), pad::LEFT_FACE_UP}},
      {id(InputAction::P2Down), {keys::S, stick(1), pad::LEFT_FACE_DOWN}},
      {id(InputAction::Launch),
       {keys::SPACE, pad::RIGHT_FACE_UP, pad::RIGHT_FACE_DOWN,
        pad::RIGHT_FACE_LEFT, pad::RIGHT_FACE_RIGHT}},
  };
}

struct Transform : BaseComponent {
  Vector2Type position;
  Vector2Type size;
  Vector2Type prev;

  void update(const Vector2Type &v) {
    prev = position;
    position = v;
  }
  Transform(Vector2Type pos, Vector2Type sz)
      : position(pos), size(sz), prev(pos) {}
  RectangleType rect() const {
    return RectangleType{position.x, position.y, size.x, size.y};
  }
  RectangleType prev_rect() const {
    return RectangleType{prev.x, prev.y, size.x, size.y};
  }
};

struct HasVelocity : BaseComponent {
  Vector2Type vel{};
};

struct PlayerID : BaseComponent {
  input::GamepadID id;
  PlayerID(input::GamepadID i) : id(i) {}
};

struct Ball : BaseComponent {};

struct ImpulseBall : System<HasVelocity, Ball> {
  input::PossibleInputCollector inpc;

  void once(const float) override {
    inpc = input::get_input_collector();
  }

  // Any player may serve: there is no serve ownership in pong, so Launch
  // deliberately ignores the action's player id (unlike ImpulsePaddle).
  virtual void for_each_with(Entity &, HasVelocity &vel, Ball &,
                             const float) override {
    if (!inpc.has_value())
      return;
    const bool space = std::ranges::any_of(inpc.inputs_pressed(), [](auto &a) {
      return a.action == static_cast<int>(InputAction::Launch);
    });

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

struct MovePaddle : System<Transform, HasVelocity, PlayerID> {
  virtual void for_each_with(Entity &, Transform &transform, HasVelocity &vel,
                             PlayerID &, const float dt) override {
    Vector2Type p = transform.position;
    p.y = std::clamp(p.y + vel.vel.y * PADDLE_SPEED * dt, 0.f,
                     (float)GAME_H - transform.size.y);
    transform.update(p);
  }
};

struct MoveBall : System<Transform, HasVelocity, Ball> {
  virtual void for_each_with(Entity &, Transform &transform, HasVelocity &vel,
                             Ball &, const float dt) override {
    Vector2Type p = transform.position;
    p.x += vel.vel.x * BALL_SPEED * dt;
    p.y += vel.vel.y * BALL_SPEED * dt;

    if (p.y < 0 || p.y + transform.size.y > GAME_H) {
      vel.vel.y *= -1;
      if (p.y < 0)
        p.y = -p.y;
      else
        p.y = 2 * (GAME_H - transform.size.y) - p.y;
    }

    if (p.x + transform.size.x < 0 || p.x > GAME_W) {
      afterhours::PlaySound(score_sound);
      GameState &state = *EntityHelper::get_singleton_cmp<GameState>();
      if (p.x + transform.size.x < 0)
        state.right++;
      else
        state.left++;
      state.game_over = state.left >= WIN_SCORE || state.right >= WIN_SCORE;
      p = kCenter;
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
      // Keyboard input arrives once per pad id, so accept any keyboard input
      // for this paddle's action; gamepad input must match its pad id.
      if (actions_done.id != playerID.id &&
          actions_done.medium != input::DeviceMedium::Keyboard)
        continue;
      if (actions_done.action == up_action)
        up = true;
      else if (actions_done.action == down_action)
        down = true;
    }

    vel.vel.y = static_cast<float>(down) - static_cast<float>(up);
  }
};

struct Collide : System<Transform, HasVelocity, Ball> {
  RefEntities colliders;

  void once(const float) override {
    colliders = EntityQuery().whereHasComponent<PlayerID>().gen();
  }

  virtual void for_each_with(Entity &, Transform &transform,
                             HasVelocity &hasVel, Ball &,
                             const float) override {
    const auto swept = rect::bounds(transform.rect(), transform.prev_rect());

    for (auto &ref : colliders) {
      Entity &other_e = ref.get();
      const Transform &other = other_e.get<Transform>();
      if (!rect::overlaps(swept, other.rect()))
        continue;

      const auto c = rect::center(transform.rect());
      const auto oc = rect::center(other.rect());
      const Vector2Type d{c.x - oc.x, c.y - oc.y};
      bool approaching =
          (hasVel.vel.x > 0 && d.x < 0) || (hasVel.vel.x < 0 && d.x > 0);
      if (!approaching)
        continue;

      hasVel.vel.x *= -1.f;

      const float offset = d.y / (other.size.y / 2.f);
      const float paddle_vy = other_e.get<HasVelocity>().vel.y;
      hasVel.vel.y = std::clamp(offset + paddle_vy * 0.2f, -kMaxBallVelY,
                                kMaxBallVelY);
      float dir = hasVel.vel.x > 0 ? 1.f : -1.f;
      hasVel.vel.x =
          dir * sqrtf(std::max(0.f, 1.f - hasVel.vel.y * hasVel.vel.y));

      Vector2Type p = transform.position;
      p.x = hasVel.vel.x > 0 ? other.position.x + other.size.x
                             : other.position.x - transform.size.x;
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

  Vector2Type position = {.x = id == 0 ? 150.f : (float)GAME_W - 180.f,
                     .y = (float)GAME_H / 2.f};

  entity.addComponent<PlayerID>(id);
  entity.addComponent<Transform>(position, Vector2Type{30.f, 150.f});
  entity.addComponent<HasVelocity>();
  entity.addComponent<HasColor>(kRayWhite);
}

Entity &make_ball() {
  auto &entity = EntityHelper::createEntity();

  entity.addComponent<Transform>(kCenter, Vector2Type{30.f, 30.f});
  entity.addComponent<HasVelocity>();
  entity.addComponent<HasColor>(kYellow);
  entity.addComponent<Ball>();
  return entity;
}

// Pong's e2e commands; included here because they inspect the game types.
#include "e2e_commands.h"

int main(int argc, char *argv[]) {
  crash::install_handler();
  SystemManager systems;
  GameState *state = nullptr;
  Entity *ball = nullptr;

  graphics::RunConfig cfg;
  cfg.width = cfg.canvas_width = GAME_W;
  cfg.height = cfg.canvas_height = GAME_H;
  cfg.title = "pong-afterhours";
  cfg.target_fps = 200;
  cfg.flags = graphics::FLAG_WINDOW_RESIZABLE | graphics::FLAG_VSYNC_HINT |
              graphics::FLAG_WINDOW_HIGHDPI;

  cfg.init = [&] {
    graphics::set_exit_key(0);
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
    EntityHelper::registerSingleton<GameState>(singletons);

    make_paddle(0);
    make_paddle(1);
    ball = &make_ball();

    // Entities are created in a temp array; tick() normally merges them, but
    // the game starts paused (no tick yet) while render already needs them.
    EntityHelper::merge_entity_arrays();

    window_manager::enforce_singletons(systems);
    input::enforce_singletons(systems);

    // External plugins first: gameplay systems read this frame's input.
    input::register_update_systems(systems);
    window_manager::register_update_systems(systems);

    systems.register_update_system(std::make_unique<ImpulseBall>());
    systems.register_update_system(std::make_unique<ImpulsePaddle>());
    systems.register_update_system(std::make_unique<MovePaddle>());
    systems.register_update_system(std::make_unique<MoveBall>());
    systems.register_update_system(std::make_unique<Collide>());

    systems.register_render_system(
        [](float) { graphics::clear_background(kDarkGray); });
    systems.register_render_system(std::make_unique<RenderScore>());
    systems.register_render_system(std::make_unique<RenderEntities>());
    systems.register_render_system(
        std::make_unique<input::RenderConnectedGamepads>());
    systems.register_render_system([](float) {
      const std::string fps = std::to_string((int)graphics::get_fps()) + " FPS";
      draw_text_right_aligned(game_font, fps.c_str(), GAME_W - 10, 0, 20,
                              kGreen);
    });
    // Last, so the menu sits on top of the ball.
    systems.register_render_system([&](float) {
      if (!state->paused)
        return;
      const char *text = state->left == 0 && state->right == 0
                             ? "PONG - P to play"
                             : "PAUSED - P to resume";
      const float w = measure_text(game_font, text, 30, 1.f).x;
      draw_text_with_shadow(game_font, text, GAME_W / 2.f - w * 0.5f,
                            GAME_H / 2.f - 20, 30, kRayWhite, kBlack);
    });
  };

  cfg.frame = [&] {
    const float dt =
        cfg.step_dt(std::min(graphics::get_frame_time(), 1.f / 30.f));
    // platform_input reads injected input under --e2e, the keyboard otherwise.
    if (testing::platform_input::is_key_pressed(keys::R)) {
      state->left = state->right = 0;
      state->game_over = false;
      state->paused = false;
      ball->get<Transform>().update(kCenter);
      ball->get<HasVelocity>().vel = {0, 0};
    }
    if ((testing::platform_input::is_key_pressed(keys::P) ||
         testing::platform_input::is_key_pressed(keys::ESCAPE)) &&
        !state->game_over)
      state->paused = !state->paused;
    if (!state->paused && !state->game_over)
      systems.tick_all(EntityHelper::get_entities_for_mod(), dt);
    systems.render_all(dt);
  };

  cfg.cleanup = [&] {
    afterhours::UnloadSound(hit_sound);
    afterhours::UnloadSound(score_sound);
    afterhours::CloseAudioDevice();
  };

  auto e2e = testing::E2EApp::attach(cfg, argc, argv,
                                     pong_e2e::register_pong_commands,
                                     "tests/e2e/screenshots");
  // Seeded so the serve angles, and so the rally scripts, repeat exactly.
  if (e2e)
    e2e->set_seed(0xE2E);
  if (!graphics::run(cfg))
    return 1;
  return e2e ? e2e->exit_code : 0;
}

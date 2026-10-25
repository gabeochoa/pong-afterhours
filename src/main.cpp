

#include "backward/backward.hpp"

namespace backward {
backward::SignalHandling sh;
} // namespace backward
#include "std_include.h"
//

#include "rl.h"

#define AFTER_HOURS_INCLUDE_DERIVED_CHILDREN
#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "afterhours/ah.h"
#define AFTER_HOURS_USE_RAYLIB
#include "afterhours/src/developer.h"
#include "afterhours/src/plugins/input_system.h"
#include "afterhours/src/plugins/window_manager.h"
#include <cassert>

//
using namespace afterhours;

typedef raylib::Vector2 vec2;
typedef raylib::Vector3 vec3;
typedef raylib::Vector4 vec4;

namespace myutil {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

template <typename T> int sgn(T val) { return (T(0) < val) - (val < T(0)); }
} // namespace myutil

constexpr int WIN_SCORE = 5;
constexpr float BALL_SPEED = 500.f;
constexpr float PADDLE_SPEED = 450.f;
constexpr int GAME_W = 1280;
constexpr int GAME_H = 720;

struct GameState : BaseComponent {
  int left = 0;
  int right = 0;
  bool game_over = false;
  bool paused = true;
};

struct RenderScore : System<GameState> {
  virtual void for_each_with(const Entity &, const GameState &state,
                             float) const override {
    raylib::DrawText(
        raylib::TextFormat("%d : %d", state.left, state.right),
        GAME_W / 2 - 30, 10, 30, raylib::RAYWHITE);
    if (state.game_over) {
      raylib::DrawText(
          raylib::TextFormat("%s WINS - R to play again",
                             state.left > state.right ? "P1" : "P2"),
          GAME_W / 2 - 190, GAME_H / 2 - 40, 30, raylib::RAYWHITE);
    }
  }
};

struct RenderFPS : System<window_manager::ProvidesCurrentResolution> {
  virtual ~RenderFPS() {}
  virtual void for_each_with(
      const Entity &,
      const window_manager::ProvidesCurrentResolution &pCurrentResolution,
      float) const override {
    raylib::DrawFPS((int)(pCurrentResolution.width() - 80), 0);
  }
};

const vec2 button_size = vec2{100, 50};

enum class InputAction {
  None,
  P1Up,
  P1Down,
  P2Up,
  P2Down,
  Launch,
};

using afterhours::input;

auto get_mapping() {
  std::map<InputAction, input::ValidInputs> mapping;
  mapping[InputAction::P1Up] = {
      raylib::KEY_UP,
      input::GamepadAxisWithDir{
          .axis = raylib::GAMEPAD_AXIS_LEFT_Y,
          .dir = -1,
      },
      raylib::GAMEPAD_BUTTON_LEFT_FACE_UP,
  };

  mapping[InputAction::P1Down] = {
      raylib::KEY_DOWN,
      input::GamepadAxisWithDir{
          .axis = raylib::GAMEPAD_AXIS_LEFT_Y,
          .dir = 1,
      },
      raylib::GAMEPAD_BUTTON_LEFT_FACE_DOWN,
  };

  mapping[InputAction::P2Up] = {
      raylib::KEY_W,
      input::GamepadAxisWithDir{
          .axis = raylib::GAMEPAD_AXIS_LEFT_Y,
          .dir = -1,
      },
      raylib::GAMEPAD_BUTTON_LEFT_FACE_UP,
  };

  mapping[InputAction::P2Down] = {
      raylib::KEY_S,
      input::GamepadAxisWithDir{
          .axis = raylib::GAMEPAD_AXIS_LEFT_Y,
          .dir = 1,
      },
      raylib::GAMEPAD_BUTTON_LEFT_FACE_DOWN,
  };

  mapping[InputAction::Launch] = {
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

  raylib::Rectangle focus_rect(int rw = 2) const {
    return raylib::Rectangle{position.x - (float)rw, position.y - (float)rw,
                             size.x + (2.f * (float)rw),
                             size.y + (2.f * (float)rw)};
  }
};

struct HasVelocity : BaseComponent {
  vec2 vel{};
};

struct HasColor : BaseComponent {
  raylib::Color color;
  HasColor(raylib::Color c) : color(c) {}
};

struct PlayerID : BaseComponent {
  input::GamepadID id;
  PlayerID(input::GamepadID i) : id(i) {}
};

struct ImpulseBall : System<HasVelocity> {
  input::PossibleInputCollector<InputAction> inpc;

  void once(float) override {
    inpc = input::get_input_collector<InputAction>();
  }

  // Any player may serve: there is no serve ownership in pong, so Launch
  // deliberately ignores the action's player id (unlike ImpulsePaddle).
  virtual void for_each_with(Entity &entity, HasVelocity &vel, float) override {
    if (entity.has<PlayerID>())
      return;

    if (!inpc.has_value()) {
      return;
    }
    bool space = false;

    for (auto &actions_done : inpc.inputs_pressed()) {
      if (actions_done.action == InputAction::Launch)
        space = true;
    }

    if (space && vel.vel.x == 0.f && vel.vel.y == 0.f) {
      float angle = (float)raylib::GetRandomValue(-35, 35) * DEG2RAD;
      float dir = raylib::GetRandomValue(0, 1) == 0 ? -1.f : 1.f;
      vel.vel.x = dir * cosf(angle);
      vel.vel.y = sinf(angle);
    }
  }
};

struct MoveAndBounce : System<Transform, HasVelocity> {
  virtual void for_each_with(Entity &entity, Transform &transform,
                             HasVelocity &vel, float dt) override {
    float speed = entity.has<PlayerID>() ? PADDLE_SPEED : BALL_SPEED;
    vec2 p = transform.pos();
    p += vel.vel * speed * dt;

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
      if (scored) {
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
  input::PossibleInputCollector<InputAction> inpc;

  void once(float) override {
    inpc = input::get_input_collector<InputAction>();
  }

  virtual void for_each_with(Entity &, HasVelocity &vel, PlayerID &playerID,
                             float) override {

    if (!inpc.has_value()) {
      vel.vel.y = 0.f;
      return;
    }

    bool up = false;
    bool down = false;

    InputAction up_action =
        playerID.id == 0 ? InputAction::P1Up : InputAction::P2Up;
    InputAction down_action =
        playerID.id == 0 ? InputAction::P1Down : InputAction::P2Down;

    for (auto &actions_done : inpc.inputs()) {
      // Keyboard actions are tagged id -1; gamepad actions carry the pad id.
      if (actions_done.id != playerID.id && actions_done.id != -1)
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

  void once(float) override {
    colliders = EntityQuery().whereHasComponent<Transform>().gen();
  }

  virtual void for_each_with(Entity &entity, Transform &transform,
                             HasVelocity &hasVel, float) override {
    if (entity.has<PlayerID>())
      return;

    raylib::Rectangle swept = transform.rect();
    swept.x = std::min(transform.pos().x, transform.prev.x);
    swept.y = std::min(transform.pos().y, transform.prev.y);
    swept.width = std::max(transform.pos().x + transform.size.x,
                           transform.prev.x + transform.size.x) -
                  swept.x;
    swept.height = std::max(transform.pos().y + transform.size.y,
                            transform.prev.y + transform.size.y) -
                   swept.y;

    for (auto &ref : colliders) {
      Entity &other_e = ref.get();
      if (other_e.id == entity.id)
        continue;
      const Transform &other = other_e.get<Transform>();
      bool overlap =
          swept.x < other.pos().x + other.size.x &&
          other.pos().x < swept.x + swept.width &&
          swept.y < other.pos().y + other.size.y &&
          other.pos().y < swept.y + swept.height;
      if (!overlap)
        continue;

      float dx = (transform.pos().x + transform.size.x / 2.f) -
                 (other.pos().x + other.size.x / 2.f);
      bool approaching =
          (hasVel.vel.x > 0 && dx < 0) || (hasVel.vel.x < 0 && dx > 0);
      if (!approaching)
        continue;

      hasVel.vel.x *= -1.f;

      float offset =
          ((transform.pos().y + transform.size.y / 2.f) -
           (other.pos().y + other.size.y / 2.f)) /
          (other.size.y / 2.f);
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
      break;
    }
  }
};

struct RenderEntities : System<Transform, HasColor> {

  virtual void for_each_with(const Entity &, const Transform &transform,
                             const HasColor &color, float) const override {
    raylib::DrawRectangleRec(transform.rect(), color.color);
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

static bool load_gamepad_mappings() {
  std::string db_path =
      std::string(raylib::GetApplicationDirectory()) + "gamecontrollerdb.txt";
  std::ifstream ifs(db_path);
  if (!ifs.is_open())
    return false;
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  input::set_gamepad_mappings(buffer.str().c_str());
  return true;
}

int main(void) {
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_RESIZABLE |
                          raylib::FLAG_VSYNC_HINT |
                          raylib::FLAG_WINDOW_HIGHDPI);
  raylib::SetExitKey(raylib::KEY_NULL);
  raylib::InitWindow(GAME_W, GAME_H, "pong-afterhours");
  if (!raylib::IsWindowReady()) {
    std::cerr << "error: failed to create window\n";
    return 1;
  }

  if (!load_gamepad_mappings())
    std::cerr << "warning: gamecontrollerdb.txt not found, "
                 "continuing with default gamepad mappings\n";

  // sophie
  auto &singletons = EntityHelper::createEntity();
  {
    input::add_singleton_components<InputAction>(singletons, get_mapping());
    window_manager::add_singleton_components(singletons, 200);
    singletons.addComponent<GameState>();
  }
  GameState &state = singletons.get<GameState>();
  raylib::SetTargetFPS(
      singletons.get<window_manager::ProvidesTargetFPS>().fps);

  make_paddle(0);
  make_paddle(1);
  Entity &ball = make_ball();

  SystemManager systems;

  // debug systems
  {
    window_manager::enforce_singletons(systems);
    input::enforce_singletons<InputAction>(systems);
  }

  // External plugins first: systems run in registration order, and the
  // gameplay systems below read this frame's input collector.
  {
    input::register_update_systems<InputAction>(systems);
    window_manager::register_update_systems(systems);
  }

  systems.register_update_system(std::make_unique<ImpulseBall>());
  systems.register_update_system(std::make_unique<ImpulsePaddle>());
  systems.register_update_system(std::make_unique<MoveAndBounce>());
  systems.register_update_system(std::make_unique<Collide>());

  // renders
  {
    systems.register_render_system(
        [&]() { raylib::ClearBackground(raylib::DARKGRAY); });
    systems.register_render_system(std::make_unique<RenderScore>());
    systems.register_render_system(std::make_unique<RenderEntities>());
    // systems.register_render_system(
    // std::make_unique<input::RenderConnectedGamepads>());
    systems.register_render_system(std::make_unique<RenderFPS>());
  }

  raylib::RenderTexture2D target =
      raylib::LoadRenderTexture(GAME_W, GAME_H);

  while (!raylib::WindowShouldClose()) {
    if (raylib::IsKeyPressed(raylib::KEY_R)) {
      state.left = state.right = 0;
      state.game_over = false;
      state.paused = false;
      ball.get<Transform>().update(
          vec2{(float)GAME_W / 2.f, (float)GAME_H / 2.f});
      ball.get<HasVelocity>().vel = {0, 0};
    }
    if ((raylib::IsKeyPressed(raylib::KEY_P) ||
         raylib::IsKeyPressed(raylib::KEY_ESCAPE)) &&
        !state.game_over)
      state.paused = !state.paused;
    float dt = std::min(raylib::GetFrameTime(), 1.f / 30.f);
    if (!state.paused && !state.game_over)
      systems.tick_all(dt);
    raylib::BeginTextureMode(target);
    systems.render_all(dt);
    if (state.paused) {
      if (state.left == 0 && state.right == 0)
        raylib::DrawText("PONG - P to play", GAME_W / 2 - 140,
                         GAME_H / 2 - 20, 30, raylib::RAYWHITE);
      else
        raylib::DrawText("PAUSED - P to resume", GAME_W / 2 - 180,
                         GAME_H / 2 - 20, 30, raylib::RAYWHITE);
    }
    raylib::EndTextureMode();
    raylib::BeginDrawing();
    raylib::ClearBackground(raylib::BLACK);
    float scale =
        std::min((float)raylib::GetScreenWidth() / (float)GAME_W,
                 (float)raylib::GetScreenHeight() / (float)GAME_H);
    float dw = (float)GAME_W * scale;
    float dh = (float)GAME_H * scale;
    raylib::DrawTexturePro(
        target.texture,
        raylib::Rectangle{0.f, 0.f, (float)GAME_W, -(float)GAME_H},
        raylib::Rectangle{((float)raylib::GetScreenWidth() - dw) / 2.f,
                          ((float)raylib::GetScreenHeight() - dh) / 2.f, dw,
                          dh},
        vec2{0, 0}, 0.f, raylib::WHITE);
    raylib::EndDrawing();
  }

  raylib::UnloadRenderTexture(target);
  raylib::CloseWindow();

  return 0;
}

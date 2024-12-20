

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

constexpr float distance_sq(const vec2 a, const vec2 b) {
  return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}

namespace myutil {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

template <typename T> int sgn(T val) { return (T(0) < val) - (val < T(0)); }
} // namespace myutil

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
  PaddleUp,
  PaddleDown,
  Launch,
};

using afterhours::input;

auto get_mapping() {
  std::map<InputAction, input::ValidInputs> mapping;
  mapping[InputAction::PaddleUp] = {
      raylib::KEY_UP,
      input::GamepadAxisWithDir{
          .axis = raylib::GAMEPAD_AXIS_LEFT_Y,
          .dir = -1,
      },
      raylib::GAMEPAD_BUTTON_LEFT_FACE_UP,
  };

  mapping[InputAction::PaddleDown] = {
      raylib::KEY_DOWN,
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

  vec2 pos() const { return position; }
  void update(vec2 &v) { position = v; }
  Transform(vec2 pos, vec2 sz) : position(pos), size(sz) {}
  raylib::Rectangle rect() const {
    return raylib::Rectangle{position.x, position.y, size.x, size.y};
  }

  raylib::Rectangle focus_rect(int rw = 2) const {
    return raylib::Rectangle{position.x - (float)rw, position.y - (float)rw,
                             size.x + (2.f * (float)rw),
                             size.y + (2.f * (float)rw)};
  }
};

struct HasColor : BaseComponent {
  raylib::Color color;
  HasColor(raylib::Color c) : color(c) {}
};

struct PlayerID : BaseComponent {
  input::GamepadID id;
  PlayerID(input::GamepadID i) : id(i) {}
};

struct Move : System<Transform, PlayerID> {

  virtual void for_each_with(Entity &entity, Transform &transform,
                             PlayerID &playerID, float dt) override {

    input::PossibleInputCollector<InputAction> inpc =
        input::get_input_collector<InputAction>();
    if (!inpc.has_value()) {
      return;
    }

    bool up = false;
    bool down = false;

    for (auto &actions_done : inpc.inputs()) {
      // std::cout << actions_done.id << std::endl;
      if (actions_done.id != playerID.id)
        continue;
      switch (actions_done.action) {
      case InputAction::PaddleUp:
        up = true;
        break;
      case InputAction::PaddleDown:
        down = true;
        break;
      // case InputAction::Launch:
      // space = true;
      // break;
      default:
        break;
      }
    }

    float sz = 100;
    vec2 p = transform.pos();
    if (up)
      p -= vec2{0, sz * dt};
    if (down)
      p += vec2{0, sz * dt};
    transform.update(p);
  }
};

struct RenderEntities : System<Transform> {

  virtual void for_each_with(const Entity &, const Transform &transform,
                             float) const override {
    raylib::DrawRectangleRec(transform.rect(), raylib::RAYWHITE);
  }
};

void make_paddle(input::GamepadID id) {
  auto &entity = EntityHelper::createEntity();

  vec2 position = {.x = id == 0 ? 150.f : 1100.f, .y = 720.f / 2.f};

  entity.addComponent<PlayerID>(id);
  entity.addComponent<Transform>(position, vec2{50.f, 250.f});
}

static void load_gamepad_mappings() {
  std::ifstream ifs("gamecontrollerdb.txt");
  if (!ifs.is_open()) {
    std::cout << "DJSLKD" << std::endl;
    return;
  }
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  input::set_gamepad_mappings(buffer.str().c_str());
}

int main(void) {
  const int screenWidth = 1280;
  const int screenHeight = 720;

  raylib::InitWindow(screenWidth, screenHeight, "wm-afterhours");
  raylib::SetTargetFPS(200);

  load_gamepad_mappings();

  // sophie
  {
    auto &entity = EntityHelper::createEntity();
    input::add_singleton_components<InputAction>(entity, get_mapping());
    window_manager::add_singleton_components(entity, 200);
  }

  make_paddle(0);
  make_paddle(1);

  SystemManager systems;

  // debug systems
  {
    window_manager::enforce_singletons(systems);
    input::enforce_singletons<InputAction>(systems);
  }

  // external plugins
  {
    input::register_update_systems<InputAction>(systems);
    window_manager::register_update_systems(systems);
  }

  systems.register_update_system(std::make_unique<Move>());

  // renders
  {
    systems.register_render_system(
        [&]() { raylib::ClearBackground(raylib::DARKGRAY); });
    systems.register_render_system(std::make_unique<RenderFPS>());
    systems.register_render_system(
        std::make_unique<input::RenderConnectedGamepads>());
    systems.register_render_system(std::make_unique<RenderEntities>());
  }

  while (!raylib::WindowShouldClose()) {
    int x = 10;
    {
      int gamepad = 0;
      raylib::DrawText(raylib::TextFormat("GP%d: %s", gamepad,
                                          raylib::GetGamepadName(gamepad)),
                       10, (x++) * 40, 10, raylib::BLACK);
      raylib::DrawText(
          raylib::TextFormat("GP%d: %f", gamepad,
                             input::get_gamepad_axis_mvt(
                                 gamepad, raylib::GAMEPAD_AXIS_LEFT_Y)),
          10, (x++) * 40, 10, raylib::BLACK);
      raylib::DrawText(
          raylib::TextFormat("GP%d: %f", gamepad,
                             input::get_gamepad_axis_mvt(
                                 gamepad, raylib::GAMEPAD_AXIS_LEFT_X)),
          10, (x++) * 40, 10, raylib::BLACK);
    }
    {
      int gamepad = 1;
      raylib::DrawText(raylib::TextFormat("GP%d: %s", gamepad,
                                          raylib::GetGamepadName(gamepad)),
                       10, (x++) * 40, 10, raylib::BLACK);
      raylib::DrawText(
          raylib::TextFormat("GP%d: %f", gamepad,
                             input::get_gamepad_axis_mvt(
                                 gamepad, raylib::GAMEPAD_AXIS_LEFT_Y)),
          10, (x++) * 40, 10, raylib::BLACK);
      raylib::DrawText(
          raylib::TextFormat("GP%d: %f", gamepad,
                             input::get_gamepad_axis_mvt(
                                 gamepad, raylib::GAMEPAD_AXIS_LEFT_X)),
          10, (x++) * 40, 10, raylib::BLACK);
    }

    std::cout << "connected ? " << input::is_gamepad_available(0) << " "
              << input::is_gamepad_available(1) << " ";

    std::cout << "left face 0 "
              << raylib::IsGamepadButtonPressed(
                     0, raylib::GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    std::cout << " left face 1 "
              << raylib::IsGamepadButtonPressed(
                     1, raylib::GAMEPAD_BUTTON_LEFT_FACE_DOWN)
              << "\n";
    raylib::BeginDrawing();
    { systems.run(raylib::GetFrameTime()); }
    raylib::EndDrawing();
  }

  raylib::CloseWindow();

  return 0;
}

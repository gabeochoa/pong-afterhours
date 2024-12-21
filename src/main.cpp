

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

struct HasVelocity : BaseComponent {
  vec2 vel;
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

  virtual void for_each_with(Entity &entity, HasVelocity &vel, float) override {
    if (entity.has<PlayerID>())
      return;

    input::PossibleInputCollector<InputAction> inpc =
        input::get_input_collector<InputAction>();
    if (!inpc.has_value()) {
      return;
    }
    bool space = false;

    for (auto &actions_done : inpc.inputs()) {
      switch (actions_done.action) {
      case InputAction::Launch:
        space = true;
        break;
      default:
        break;
      }
    }

    if (space && vel.vel.x == 0.f && vel.vel.y == 0.f) {
      vel.vel.x = -0.5f;
      vel.vel.y = -0.5f * 2;
    }
  }
};

struct MoveAndBounce : System<Transform, HasVelocity> {
  float map_width;
  float map_height;

  void once(float) {
    auto &rez_manager =
        EntityQuery()
            .whereHasComponent<window_manager::ProvidesCurrentResolution>()
            .gen_first_enforce();

    window_manager::ProvidesCurrentResolution &pCurrentResolution =
        rez_manager.get<window_manager::ProvidesCurrentResolution>();

    map_width = (float)pCurrentResolution.width();
    map_height = (float)pCurrentResolution.height();

    // TODO get score singleton
  }
  virtual void for_each_with(Entity &entity, Transform &transform,
                             HasVelocity &vel, float dt) override {
    float sz = 500;
    vec2 p = transform.pos();
    p += vel.vel * sz * dt;

    if (p.y < 0 || p.y + transform.size.y > map_height) {
      // players dont bounce
      if (entity.has<PlayerID>())
        return;

      vel.vel.y *= -1;
      p += vel.vel * sz * dt;
      p += vel.vel * sz * dt;
    }

    if (p.x < -10 || p.x > map_width + 10) {
      p = vec2{map_width / 2.f, map_height / 2.f};
      vel.vel = {0, 0};
      // TODO add to score
    }

    transform.update(p);
  }
};

struct ImpulsePaddle : System<HasVelocity, PlayerID> {

  virtual void for_each_with(Entity &, HasVelocity &vel, PlayerID &playerID,
                             float) override {

    input::PossibleInputCollector<InputAction> inpc =
        input::get_input_collector<InputAction>();
    if (!inpc.has_value()) {
      return;
    }

    bool up = false;
    bool down = false;

    for (auto &actions_done : inpc.inputs()) {
      if (actions_done.id != playerID.id)
        continue;
      switch (actions_done.action) {
      case InputAction::PaddleUp:
        up = true;
        break;
      case InputAction::PaddleDown:
        down = true;
        break;
      default:
        break;
      }
    }

    if (up) {
      vel.vel.y = -1.f;
    } else if (down) {
      vel.vel.y = 1.f;
    } else {
      vel.vel.y = 0.f;
    }
  }
};

using raylib::Rectangle;
struct EQ : public EntityQuery<EQ> {
  struct WhereInRange : EntityQuery::Modification {
    vec2 position;
    float range;

    // TODO mess around with the right epsilon here
    explicit WhereInRange(vec2 pos, float r = 0.01f)
        : position(pos), range(r) {}
    bool operator()(const Entity &entity) const override {
      vec2 pos = entity.get<Transform>().pos();
      return distance_sq(position, pos) < (range * range);
    }
  };

  EQ &whereInRange(const vec2 &position, float range) {
    return add_mod(new WhereInRange(position, range));
  }

  EQ &orderByDist(const vec2 &position) {
    return orderByLambda([position](const Entity &a, const Entity &b) {
      float a_dist = distance_sq(a.get<Transform>().pos(), position);
      float b_dist = distance_sq(b.get<Transform>().pos(), position);
      return a_dist < b_dist;
    });
  }

  struct WhereOverlaps : EntityQuery::Modification {
    Rectangle rect;

    explicit WhereOverlaps(Rectangle rect_) : rect(rect_) {}

    bool operator()(const Entity &entity) const override {
      const auto r1 = rect;
      const auto r2 = entity.get<Transform>().rect();
      // Check if the rectangles overlap on the x-axis
      const bool xOverlap = r1.x < r2.x + r2.width && r2.x < r1.x + r1.width;

      // Check if the rectangles overlap on the y-axis
      const bool yOverlap = r1.y < r2.y + r2.height && r2.y < r1.y + r1.height;

      // Return true if both x and y overlap
      return xOverlap && yOverlap;
    }
  };

  EQ &whereOverlaps(const Rectangle r) { return add_mod(new WhereOverlaps(r)); }
};

struct Collide : System<Transform, HasVelocity> {
  virtual void for_each_with(Entity &entity, Transform &transform,
                             HasVelocity &hasVel, float) override {
    // only worry about those that can flip

    auto colliders = EQ().whereNotID(entity.id)
                         .whereHasComponent<Transform>()
                         .whereOverlaps(transform.rect())
                         .gen();
    if (colliders.size() == 0)
      return;

    hasVel.vel = hasVel.vel * -1.f;
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
  entity.addComponent<Transform>(position, vec2{30.f, 150.f});
  entity.addComponent<HasVelocity>();
}

void make_ball() {
  auto &entity = EntityHelper::createEntity();

  vec2 position = {.x = 1280.f / 2.f, .y = 720.f / 2.f};

  entity.addComponent<Transform>(position, vec2{30.f, 30.f});
  entity.addComponent<HasVelocity>();
}

static void load_gamepad_mappings() {
  std::ifstream ifs("gamecontrollerdb.txt");
  if (!ifs.is_open()) {
    std::cout << "Failed to load game controller db" << std::endl;
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
  make_ball();

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

  systems.register_update_system(std::make_unique<ImpulseBall>());
  systems.register_update_system(std::make_unique<ImpulsePaddle>());
  systems.register_update_system(std::make_unique<MoveAndBounce>());
  systems.register_update_system(std::make_unique<Collide>());

  // renders
  {
    systems.register_render_system(
        [&]() { raylib::ClearBackground(raylib::DARKGRAY); });
    systems.register_render_system(std::make_unique<RenderFPS>());
    // systems.register_render_system(
    // std::make_unique<input::RenderConnectedGamepads>());
    systems.register_render_system(std::make_unique<RenderEntities>());
  }

  while (!raylib::WindowShouldClose()) {
    raylib::BeginDrawing();
    { systems.run(raylib::GetFrameTime()); }
    raylib::EndDrawing();
  }

  raylib::CloseWindow();

  return 0;
}

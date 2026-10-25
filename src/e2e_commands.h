// Pong's e2e test hooks and state assertions. Assertions use cmd.expect so a
// state landing a frame late still passes; malformed commands fail().
#pragma once

// Upstream's animation header shadows a field; silence it for upstream only.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "afterhours/src/graphics.h"
#include "afterhours/src/plugins/e2e_testing/app.h"
#include "afterhours/src/plugins/e2e_testing/perf_commands.h"
#pragma GCC diagnostic pop

namespace pong_e2e {
using namespace afterhours;
using namespace afterhours::testing;

static GameState *find_state() {
  auto state = EntityQuery().whereHasComponent<GameState>().gen_first();
  return state.valid() ? &state.asE().get<GameState>() : nullptr;
}

static Entity *find_ball() {
  for (auto &ref :
       EntityQuery().whereHasComponent<Transform>().gen()) {
    Entity &e = ref.get();
    if (e.has<HasVelocity>() && !e.has<PlayerID>())
      return &e;
  }
  return nullptr;
}

static Entity *find_paddle(int id) {
  for (auto &ref : EntityQuery().whereHasComponent<PlayerID>().gen()) {
    Entity &e = ref.get();
    if (e.get<PlayerID>().id == id)
      return &e;
  }
  return nullptr;
}

struct CenterPaddlesCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd,
                             const float) override {
    if (cmd.is_consumed() || !cmd.is("center_paddles"))
      return;
    for (int id = 0; id < 2; id++) {
      if (Entity *paddle = find_paddle(id)) {
        Transform &t = paddle->get<Transform>();
        t.update(vec2{t.pos().x, (float)GAME_H / 2.f});
        t.prev = t.pos();
        paddle->get<HasVelocity>().vel = {0, 0};
      }
    }
    cmd.consume();
  }
};

struct TeleportBallCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd,
                             const float) override {
    if (cmd.is_consumed() || !cmd.is("teleport_ball"))
      return;
    auto x = cmd.maybe_arg_as<float>(0);
    auto y = cmd.maybe_arg_as<float>(1);
    auto vx = cmd.maybe_arg_as<float>(2);
    auto vy = cmd.maybe_arg_as<float>(3);
    if (!x || !y || !vx || !vy) {
      cmd.fail("teleport_ball needs x y vx vy");
      return;
    }
    Entity *ball = find_ball();
    if (!ball) {
      cmd.fail("teleport_ball: no ball");
      return;
    }
    Transform &t = ball->get<Transform>();
    t.update(vec2{*x, *y});
    t.prev = t.pos();
    ball->get<HasVelocity>().vel = vec2{*vx, *vy};
    cmd.consume();
  }
};

struct AssertPausedCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd,
                             const float) override {
    if (cmd.is_consumed() || !cmd.is("assert_paused"))
      return;
    auto want = cmd.maybe_arg_as<int>(0);
    GameState *state = find_state();
    if (!want || !state) {
      cmd.fail("assert_paused needs 0|1");
      return;
    }
    cmd.expect(state->paused == (*want != 0),
           "paused is " + std::string(state->paused ? "true" : "false"));
  }
};

struct AssertScoreCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd,
                             const float) override {
    if (cmd.is_consumed() || !cmd.is("assert_score"))
      return;
    auto left = cmd.maybe_arg_as<int>(0);
    auto right = cmd.maybe_arg_as<int>(1);
    GameState *state = find_state();
    if (!left || !right || !state) {
      cmd.fail("assert_score needs left right");
      return;
    }
    cmd.expect(state->left == *left && state->right == *right,
           "score is " + std::to_string(state->left) + ":" +
               std::to_string(state->right));
  }
};

struct AssertGameOverCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd,
                             const float) override {
    if (cmd.is_consumed() || !cmd.is("assert_game_over"))
      return;
    auto want = cmd.maybe_arg_as<int>(0);
    GameState *state = find_state();
    if (!want || !state) {
      cmd.fail("assert_game_over needs 0|1");
      return;
    }
    cmd.expect(state->game_over == (*want != 0),
           "game_over is " +
               std::string(state->game_over ? "true" : "false"));
  }
};

struct AssertBallMovingCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd,
                             const float) override {
    if (cmd.is_consumed() || !cmd.is("assert_ball_moving"))
      return;
    Entity *ball = find_ball();
    if (!ball) {
      cmd.fail("assert_ball_moving: no ball");
      return;
    }
    vec2 vel = ball->get<HasVelocity>().vel;
    cmd.expect(std::abs(vel.x) > 0.01f || std::abs(vel.y) > 0.01f,
           "ball is stopped");
  }
};

struct AssertBallStoppedCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd,
                             const float) override {
    if (cmd.is_consumed() || !cmd.is("assert_ball_stopped"))
      return;
    Entity *ball = find_ball();
    if (!ball) {
      cmd.fail("assert_ball_stopped: no ball");
      return;
    }
    vec2 vel = ball->get<HasVelocity>().vel;
    cmd.expect(std::abs(vel.x) <= 0.01f && std::abs(vel.y) <= 0.01f,
           "ball is moving");
  }
};

struct AssertBallVelCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd,
                             const float) override {
    if (cmd.is_consumed() ||
        (!cmd.is("assert_ball_vel_x") && !cmd.is("assert_ball_vel_y")))
      return;
    std::string cmp = cmd.arg(0);
    auto value = cmd.maybe_arg_as<float>(1);
    Entity *ball = find_ball();
    if ((cmp != "gt" && cmp != "lt") || !value || !ball) {
      cmd.fail("assert_ball_vel_x|y needs gt|lt value");
      return;
    }
    float vel = cmd.is("assert_ball_vel_x")
                    ? ball->get<HasVelocity>().vel.x
                    : ball->get<HasVelocity>().vel.y;
    cmd.expect(cmp == "gt" ? vel > *value : vel < *value,
           "ball vel is " + std::to_string(vel));
  }
};

struct AssertPaddleYCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd,
                             const float) override {
    if (cmd.is_consumed() || !cmd.is("assert_paddle_y"))
      return;
    auto id = cmd.maybe_arg_as<int>(0);
    std::string cmp = cmd.arg(1);
    auto value = cmd.maybe_arg_as<float>(2);
    Entity *paddle = id ? find_paddle(*id) : nullptr;
    if (!id || (cmp != "lt" && cmp != "gt") || !value || !paddle) {
      cmd.fail("assert_paddle_y needs player lt|gt value");
      return;
    }
    float y = paddle->get<Transform>().pos().y;
    cmd.expect(cmp == "lt" ? y < *value : y > *value,
           "paddle y is " + std::to_string(y));
  }
};

inline void register_pong_commands(SystemManager &systems) {
  perf_commands::set_provider(
      {.get_fps = [] { return std::optional<float>(graphics::get_fps()); }});
  perf_commands::register_perf_commands(systems);
  systems.register_update_system(std::make_unique<CenterPaddlesCommand>());
  systems.register_update_system(std::make_unique<TeleportBallCommand>());
  systems.register_update_system(std::make_unique<AssertPausedCommand>());
  systems.register_update_system(std::make_unique<AssertScoreCommand>());
  systems.register_update_system(std::make_unique<AssertGameOverCommand>());
  systems.register_update_system(
      std::make_unique<AssertBallMovingCommand>());
  systems.register_update_system(
      std::make_unique<AssertBallStoppedCommand>());
  systems.register_update_system(std::make_unique<AssertBallVelCommand>());
  systems.register_update_system(std::make_unique<AssertPaddleYCommand>());
}

} // namespace pong_e2e

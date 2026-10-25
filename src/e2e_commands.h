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
  return EntityHelper::get_singleton_cmp<GameState>();
}

static Entity *find_ball() {
  auto ball = EntityQuery().whereHasComponent<Ball>().gen_first();
  return ball.valid() ? &ball.asE() : nullptr;
}

static Entity *find_paddle(int id) {
  for (auto &ref : EntityQuery().whereHasComponent<PlayerID>().gen())
    if (ref.get().get<PlayerID>().id == id)
      return &ref.get();
  return nullptr;
}

// Moves without a swept trail (prev = pos), so no phantom collision.
static void place(Entity &e, Vector2Type pos, Vector2Type vel) {
  Transform &t = e.get<Transform>();
  t.update(pos);
  t.prev = pos;
  e.get<HasVelocity>().vel = vel;
}

// assert_<name> 0|1 against a GameState flag.
static void expect_flag(PendingE2ECommand &cmd, bool GameState::*flag) {
  auto want = cmd.maybe_arg_as<int>(0);
  GameState *state = find_state();
  if (!want || !state) {
    cmd.fail(cmd.name + " needs 0|1");
    return;
  }
  const bool v = state->*flag;
  cmd.expect(v == (*want != 0),
             cmd.name + ": is " + std::string(v ? "true" : "false"));
}

static void expect_ball_moving(PendingE2ECommand &cmd, bool want) {
  Entity *ball = find_ball();
  if (!ball) {
    cmd.fail(cmd.name + ": no ball");
    return;
  }
  const Vector2Type vel = ball->get<HasVelocity>().vel;
  const bool moving = std::abs(vel.x) > 0.01f || std::abs(vel.y) > 0.01f;
  cmd.expect(moving == want, moving ? "ball is moving" : "ball is stopped");
}

// assert_ball_vel_x|y gt|lt value
static void expect_ball_vel(PendingE2ECommand &cmd, float Vector2Type::*axis) {
  const std::string cmp = cmd.arg(0);
  auto value = cmd.maybe_arg_as<float>(1);
  Entity *ball = find_ball();
  if ((cmp != "gt" && cmp != "lt") || !value || !ball) {
    cmd.fail(cmd.name + " needs gt|lt value");
    return;
  }
  const float vel = ball->get<HasVelocity>().vel.*axis;
  cmd.expect(cmp == "gt" ? vel > *value : vel < *value,
             "ball vel is " + std::to_string(vel));
}

inline void register_pong_commands(SystemManager &sm) {
  perf_commands::set_provider(
      {.get_fps = [] { return std::optional<float>(graphics::get_fps()); }});
  perf_commands::register_perf_commands(sm);

  register_command(sm, "center_paddles", [](PendingE2ECommand &cmd) {
    for (int id = 0; id < 2; id++)
      if (Entity *paddle = find_paddle(id))
        place(*paddle,
              {paddle->get<Transform>().position.x, (float)GAME_H / 2.f},
              {0, 0});
    cmd.consume();
  });
  register_command(sm, "teleport_ball", [](PendingE2ECommand &cmd) {
    auto x = cmd.maybe_arg_as<float>(0), y = cmd.maybe_arg_as<float>(1);
    auto vx = cmd.maybe_arg_as<float>(2), vy = cmd.maybe_arg_as<float>(3);
    Entity *ball = find_ball();
    if (!x || !y || !vx || !vy || !ball) {
      cmd.fail("teleport_ball needs x y vx vy (and a ball)");
      return;
    }
    place(*ball, {*x, *y}, {*vx, *vy});
    cmd.consume();
  });
  register_command(sm, "assert_paused", [](PendingE2ECommand &cmd) {
    expect_flag(cmd, &GameState::paused);
  });
  register_command(sm, "assert_game_over", [](PendingE2ECommand &cmd) {
    expect_flag(cmd, &GameState::game_over);
  });
  register_command(sm, "assert_score", [](PendingE2ECommand &cmd) {
    auto left = cmd.maybe_arg_as<int>(0), right = cmd.maybe_arg_as<int>(1);
    GameState *state = find_state();
    if (!left || !right || !state) {
      cmd.fail("assert_score needs left right");
      return;
    }
    cmd.expect(state->left == *left && state->right == *right,
               "score is " + std::to_string(state->left) + ":" +
                   std::to_string(state->right));
  });
  register_command(sm, "assert_ball_moving", [](PendingE2ECommand &cmd) {
    expect_ball_moving(cmd, true);
  });
  register_command(sm, "assert_ball_stopped", [](PendingE2ECommand &cmd) {
    expect_ball_moving(cmd, false);
  });
  register_command(sm, "assert_ball_vel_x", [](PendingE2ECommand &cmd) {
    expect_ball_vel(cmd, &Vector2Type::x);
  });
  register_command(sm, "assert_ball_vel_y", [](PendingE2ECommand &cmd) {
    expect_ball_vel(cmd, &Vector2Type::y);
  });
  register_command(sm, "assert_paddle_y", [](PendingE2ECommand &cmd) {
    auto id = cmd.maybe_arg_as<int>(0);
    const std::string cmp = cmd.arg(1);
    auto value = cmd.maybe_arg_as<float>(2);
    Entity *paddle = id ? find_paddle(*id) : nullptr;
    if ((cmp != "lt" && cmp != "gt") || !value || !paddle) {
      cmd.fail("assert_paddle_y needs player lt|gt value");
      return;
    }
    const float y = paddle->get<Transform>().position.y;
    cmd.expect(cmp == "lt" ? y < *value : y > *value,
               "paddle y is " + std::to_string(y));
  });
}

} // namespace pong_e2e

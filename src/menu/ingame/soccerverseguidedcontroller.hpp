#ifndef _HPP_MENU_INGAME_SOCCERVERSEGUIDEDCONTROLLER
#define _HPP_MENU_INGAME_SOCCERVERSEGUIDEDCONTROLLER

#include "../../onthepitch/AIsupport/AIfunctions.hpp"
#include "../../onthepitch/match.hpp"
#include "../../onthepitch/player/player.hpp"
#include "managers/environmentmanager.hpp"

using namespace blunted;

enum class SoccerverseGuideRole {
  Shape,
  Passer,
  RunnerShooter,
};

// A thin director layer over League-Soccer's native AI. The fallback controller keeps
// running every tick. This controller only replaces selected movement/actions around a
// Soccerverse factual anchor, so native locomotion, ball physics and football AI remain live.
class SoccerverseGuidedController : public IController {
public:
  SoccerverseGuidedController(Match* match, int formationSlot, bool attackingMentality,
                              SoccerverseGuideRole guideRole, Player* sourceTarget,
                              unsigned long directorStart_ms)
      : IController(match),
        formationSlot(formationSlot),
        attackingMentality(attackingMentality),
        guideRole(guideRole),
        sourceTarget(sourceTarget),
        directorStart_ms(directorStart_ms),
        lastDirection(1.0f, 0.0f, 0.0f),
        lastVelocity(0.0f) {}

  void Process() override {
    // External controllers replace the normal controller's Process() call in PlayerBase.
    // Keep the native Eliza controller hot so its mental image / strategies remain valid.
    if (fallbackController)
      fallbackController->Process();
  }

  void RequestCommand(PlayerCommandQueue& commandQueue) override {
    if (!player)
      return;

    Player* guidedPlayer = static_cast<Player*>(player);
    if (fallbackController)
      fallbackController->RequestCommand(commandQueue);

    const unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();
    const unsigned long elapsed_ms = now_ms >= directorStart_ms ? now_ms - directorStart_ms : 0;
    const Vector3 ballPosition = match->GetBall()->Predict(0).Get2D();

    // Source anchor: 68' Torino White - Kalulu slides the ball across to Vlahovic.
    if (guideRole == SoccerverseGuideRole::Passer && sourceTarget && elapsed_ms >= 1800 &&
        elapsed_ms <= 5200 &&
        ballPosition.GetDistance(guidedPlayer->GetPosition().Get2D()) < 2.6f) {
      ForcePass(commandQueue, guidedPlayer, sourceTarget, e_FunctionType_LongPass);
      return;
    }

    // Source anchor: Vlahovic chases it through and scores. Before receiving, drive a
    // football-shaped run through the channel; once the ball arrives, request a native shot.
    if (guideRole == SoccerverseGuideRole::RunnerShooter) {
      const float side = static_cast<float>(guidedPlayer->GetTeam()->GetSide());
      const Vector3 runTarget(-side * 39.0f, 5.5f, 0.0f);
      const float ballDistance = ballPosition.GetDistance(guidedPlayer->GetPosition().Get2D());

      if (elapsed_ms >= 3000 && elapsed_ms <= 9000 && ballDistance < 2.3f) {
        ForceShot(commandQueue, guidedPlayer);
        return;
      }

      if (elapsed_ms >= 900 && elapsed_ms <= 6500) {
        OverrideMovement(commandQueue, guidedPlayer, runTarget, sprintVelocity);
        return;
      }
    }

    // All 22 players keep the native AI, but when an off-ball player drifts too far from
    // the reconstructed 4-4-2 block we replace only the movement command. Near the ball,
    // League-Soccer is free to press, tackle, support and react normally.
    if (!guidedPlayer->HasPossession() &&
        ballPosition.GetDistance(guidedPlayer->GetPosition().Get2D()) > 3.5f) {
      const Vector3 shapeTarget = GetDynamic442Target(guidedPlayer, ballPosition);
      const float shapeDistance = shapeTarget.GetDistance(guidedPlayer->GetPosition().Get2D());
      if (shapeDistance > 5.0f) {
        const float desiredVelocity = shapeDistance > 12.0f ? sprintVelocity : walkVelocity;
        OverrideMovement(commandQueue, guidedPlayer, shapeTarget, desiredVelocity);
      }
    }
  }

  Vector3 GetDirection() override {
    if (fallbackController)
      return fallbackController->GetDirection();
    return lastDirection;
  }

  float GetFloatVelocity() override {
    if (fallbackController)
      return fallbackController->GetFloatVelocity();
    return lastVelocity;
  }

  void Reset() override {
    lastDirection = Vector3(1.0f, 0.0f, 0.0f);
    lastVelocity = idleVelocity;
  }

private:
  Vector3 GetBase442Target(Player* guidedPlayer) const {
    const float side = static_cast<float>(guidedPlayer->GetTeam()->GetSide());

    // Formation #12 in the fixture capture is mapped to a 4-4-2 for this vertical slice.
    // The mapping is reconstructed from the historical XI/role slots; these are formation
    // geometry anchors, not claimed historical XY tracking coordinates.
    switch (formationSlot) {
      case 0:
        return Vector3(side * 51.0f, 0.0f, 0.0f);       // GK
      case 1:
        return Vector3(side * 35.0f, -24.0f, 0.0f);     // RB
      case 2:
        return Vector3(side * 38.0f, -8.0f, 0.0f);      // RCB
      case 3:
        return Vector3(side * 38.0f, 8.0f, 0.0f);       // LCB
      case 4:
        return Vector3(side * 35.0f, 24.0f, 0.0f);      // LB
      case 5:
        return Vector3(side * 9.0f, -25.0f, 0.0f);      // RM
      case 6:
        return Vector3(side * 10.0f, -8.0f, 0.0f);      // RCM
      case 7:
        return Vector3(side * 10.0f, 8.0f, 0.0f);       // LCM
      case 8:
        return Vector3(side * 9.0f, 25.0f, 0.0f);       // LM
      case 9:
        return Vector3(side * -20.0f, -8.0f, 0.0f);     // RF
      case 10:
        return Vector3(side * -20.0f, 8.0f, 0.0f);      // LF
      default:
        return guidedPlayer->GetPosition().Get2D();
    }
  }

  Vector3 GetDynamic442Target(Player* guidedPlayer, const Vector3& ballPosition) const {
    Vector3 target = GetBase442Target(guidedPlayer);
    if (formationSlot == 0) {
      target.coords[1] = clamp(ballPosition.coords[1] * 0.10f, -3.0f, 3.0f);
      return target;
    }

    // Whole-block translation toward the ball while keeping the line geometry recognizable.
    target.coords[0] += ballPosition.coords[0] * 0.18f;
    target.coords[1] += ballPosition.coords[1] * 0.22f;

    if (attackingMentality) {
      const float side = static_cast<float>(guidedPlayer->GetTeam()->GetSide());
      const float mentalityPush = formationSlot >= 9 ? 5.0f : (formationSlot >= 5 ? 3.0f : 1.5f);
      target.coords[0] += -side * mentalityPush;
    }

    target.coords[0] = clamp(target.coords[0], -50.0f, 50.0f);
    target.coords[1] = clamp(target.coords[1], -32.0f, 32.0f);
    return target;
  }

  void OverrideMovement(PlayerCommandQueue& commandQueue, Player* guidedPlayer,
                        const Vector3& target, float desiredVelocity) {
    PlayerCommand movement;
    movement.desiredFunctionType = e_FunctionType_Movement;
    movement.useDesiredMovement = true;
    movement.desiredDirection =
        (target - guidedPlayer->GetPosition().Get2D()).GetNormalized(guidedPlayer->GetDirectionVec());
    movement.desiredVelocityFloat = desiredVelocity;
    movement.useDesiredLookAt = true;
    movement.desiredLookAt = match->GetBall()->Predict(80).Get2D();

    bool replaced = false;
    for (auto it = commandQueue.rbegin(); it != commandQueue.rend(); ++it) {
      if (it->desiredFunctionType == e_FunctionType_Movement) {
        *it = movement;
        replaced = true;
        break;
      }
    }
    if (!replaced)
      commandQueue.push_back(movement);

    lastDirection = movement.desiredDirection;
    lastVelocity = desiredVelocity;
  }

  void ForcePass(PlayerCommandQueue& commandQueue, Player* passer, Player* target,
                 e_FunctionType passType) {
    PlayerCommand pass;
    pass.desiredFunctionType = passType;
    pass.useDesiredMovement = false;
    pass.useDesiredLookAt = false;
    pass.touchInfo.targetPlayer = nullptr;
    pass.touchInfo.forcedTargetPlayer = target;
    pass.touchInfo.inputDirection = Vector3(0.0f);
    pass.touchInfo.inputPower = 0.0f;
    pass.touchInfo.autoDirectionBias = 1.0f;
    pass.touchInfo.autoPowerBias = 1.0f;
    AI_GetPass(passer, passType, pass.touchInfo.inputDirection, pass.touchInfo.inputPower,
               pass.touchInfo.autoDirectionBias, pass.touchInfo.autoPowerBias,
               pass.touchInfo.desiredDirection, pass.touchInfo.desiredPower,
               pass.touchInfo.targetPlayer, pass.touchInfo.forcedTargetPlayer);

    // Native Eliza queues actions before its final movement command. Preserve that ordering.
    commandQueue.insert(commandQueue.begin(), pass);
  }

  void ForceShot(PlayerCommandQueue& commandQueue, Player* shooter) {
    const float side = static_cast<float>(shooter->GetTeam()->GetSide());
    const Vector3 goalTarget(-side * (pitchHalfW + 1.0f), 2.4f, 0.0f);

    PlayerCommand shot;
    shot.desiredFunctionType = e_FunctionType_Shot;
    shot.useDesiredMovement = false;
    shot.useDesiredLookAt = false;
    shot.desiredVelocityFloat = shooter->GetFloatVelocity();
    shot.touchInfo.desiredDirection =
        (goalTarget - shooter->GetPosition()).GetNormalized(Vector3(-side, 0.0f, 0.0f));
    shot.touchInfo.autoDirectionBias = 1.0f;
    shot.touchInfo.desiredPower = 0.92f;
    commandQueue.insert(commandQueue.begin(), shot);
  }

  int formationSlot;
  bool attackingMentality;
  SoccerverseGuideRole guideRole;
  Player* sourceTarget;
  unsigned long directorStart_ms;
  Vector3 lastDirection;
  float lastVelocity;
};

#endif  // _HPP_MENU_INGAME_SOCCERVERSEGUIDEDCONTROLLER

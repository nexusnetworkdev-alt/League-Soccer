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

// Thin Soccerverse director layer. Stability rule for v2.0.1: only the factual
// event actors remain externally guided. Shape-only players detach immediately
// and continue under League-Soccer's untouched native AI.
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
    // Do not replace the native controller for off-ball shape players. The first
    // tick returns them to League-Soccer AI, reducing the external-controller
    // surface from 22 players to the two source-event actors only.
    if (guideRole == SoccerverseGuideRole::Shape) {
      if (player)
        player->SetExternalController(nullptr);
      return;
    }

    // The two guided actors still need the native controller to update its
    // mental image and strategies before we add Soccerverse anchor commands.
    if (fallbackController)
      fallbackController->Process();
  }

  void RequestCommand(PlayerCommandQueue& commandQueue) override {
    if (!player)
      return;

    Player* guidedPlayer = static_cast<Player*>(player);
    if (fallbackController)
      fallbackController->RequestCommand(commandQueue);

    // A shape controller should have detached in Process(). Keep this guard so
    // an unusual call order still behaves as a pure native-AI passthrough.
    if (guideRole == SoccerverseGuideRole::Shape)
      return;

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

    // Source anchor: Vlahovic chases it through and scores. Before receiving,
    // drive one native run through the channel; once the ball arrives, request
    // a native shot. Everybody else is entirely controlled by League-Soccer.
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

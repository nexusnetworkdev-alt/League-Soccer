#ifndef _HPP_MENU_INGAME_SOCCERVERSEGUIDEDCONTROLLER
#define _HPP_MENU_INGAME_SOCCERVERSEGUIDEDCONTROLLER

#include "../../onthepitch/match.hpp"
#include "../../onthepitch/player/player.hpp"

using namespace blunted;

enum class SoccerverseGuideRole {
  Shape,
  Passer,
  RunnerShooter,
};

// Stability probe for the Soccerverse vertical slice.
//
// The first v2 builds proved that replacing League-Soccer controllers is not
// safe enough yet: opening the Soccerverse page can crash before the guided
// action starts. For this probe the external controller is deliberately a
// zero-logic handoff. At the first engine callback it immediately restores the
// player's untouched native controller and forwards that same callback to it.
//
// This keeps all 22 players under League-Soccer AI and lets us isolate the
// remaining Director layer (formation staging, ball anchor, camera and hard
// outcome) without custom PlayerCommand generation.
class SoccerverseGuidedController : public IController {
public:
  SoccerverseGuidedController(Match* match, int formationSlot, bool attackingMentality,
                              SoccerverseGuideRole guideRole, Player* sourceTarget,
                              unsigned long directorStart_ms)
      : IController(match), lastDirection(1.0f, 0.0f, 0.0f), lastVelocity(0.0f) {
    (void)formationSlot;
    (void)attackingMentality;
    (void)guideRole;
    (void)sourceTarget;
    (void)directorStart_ms;
  }

  void Process() override {
    IController* nativeController = fallbackController;
    if (player)
      player->SetExternalController(nullptr);
    if (nativeController)
      nativeController->Process();
  }

  void RequestCommand(PlayerCommandQueue& commandQueue) override {
    IController* nativeController = fallbackController;
    if (player)
      player->SetExternalController(nullptr);
    if (nativeController)
      nativeController->RequestCommand(commandQueue);
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
  Vector3 lastDirection;
  float lastVelocity;
};

#endif  // _HPP_MENU_INGAME_SOCCERVERSEGUIDEDCONTROLLER

#ifndef _HPP_MENU_INGAME_SOCCERVERSEGUIDEDCONTROLLER
#define _HPP_MENU_INGAME_SOCCERVERSEGUIDEDCONTROLLER

#include "../../onthepitch/match.hpp"
#include "../../onthepitch/player/player.hpp"
#include "managers/environmentmanager.hpp"

using namespace blunted;

enum class SoccerverseGuideRole {
  Shape,
  Passer,
  RunnerShooter,
};

// Soccerverse event-sandbox controller.
//
// Stability rule: League-Soccer keeps full ownership of player movement and
// PlayerCommand generation. Shape players and the runner immediately return to
// their untouched native controllers. Only the factual passer keeps this thin
// wrapper for a few seconds, forwarding native AI calls unchanged while the
// Director injects two one-shot BALL anchors:
//   Kalulu -> Vlahovic, then Vlahovic -> goal.
//
// No custom player commands are generated here.
class SoccerverseGuidedController : public IController {
public:
  SoccerverseGuidedController(Match* match, int formationSlot, bool attackingMentality,
                              SoccerverseGuideRole guideRole, Player* sourceTarget,
                              unsigned long directorStart_ms)
      : IController(match),
        guideRole(guideRole),
        sourceTarget(sourceTarget),
        directorStart_ms(directorStart_ms),
        passAnchorSent(false),
        shotAnchorSent(false),
        lastDirection(1.0f, 0.0f, 0.0f),
        lastVelocity(0.0f) {
    (void)formationSlot;
    (void)attackingMentality;
  }

  void Process() override {
    // Every player except the factual passer is pure native League-Soccer AI.
    if (guideRole != SoccerverseGuideRole::Passer) {
      IController* nativeController = fallbackController;
      if (player)
        player->SetExternalController(nullptr);
      if (nativeController)
        nativeController->Process();
      return;
    }

    // Keep Kalulu's native AI fully alive. This wrapper never replaces his
    // movement/pass commands; it only provides the factual ball trajectory.
    if (fallbackController)
      fallbackController->Process();

    if (!player || !sourceTarget)
      return;

    const unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();
    const unsigned long elapsed_ms = now_ms >= directorStart_ms ? now_ms - directorStart_ms : 0;
    Player* passer = static_cast<Player*>(player);
    const float side = static_cast<float>(passer->GetTeam()->GetSide());

    // Anchor 1: make the actual Kalulu -> Vlahovic pass visible. This is a
    // single launch, not per-frame ball tracking, so defenders and both teams
    // can still react through the native simulation.
    if (!passAnchorSent && elapsed_ms >= 1200) {
      const Vector3 origin = passer->GetPosition() + Vector3(-side * 0.50f, 0.0f, 0.13f);
      const Vector3 target = sourceTarget->GetPosition() + Vector3(-side * 1.2f, 0.0f, 0.10f);
      const Vector3 passVelocity =
          (target - origin).GetNormalized(Vector3(-side, 0.0f, 0.0f)) * 18.0f;
      match->SetBallRetainer(nullptr);
      match->GetBall()->SetPosition(origin);
      match->GetBall()->SetMomentum(passVelocity);
      passAnchorSent = true;
      match->SpamMessage("SV anchor: Kalulu -> Vlahovic", 1800);
    }

    // Anchor 2: after the pass has travelled, reconcile the ball once at
    // Vlahovic's current feet and launch the factual shot. The runner himself
    // remains under the engine's normal AI/animation system.
    if (!shotAnchorSent && elapsed_ms >= 3800) {
      const Vector3 origin =
          sourceTarget->GetPosition() + Vector3(-side * 0.52f, 0.0f, 0.14f);
      const Vector3 goalTarget(-side * (pitchHalfW + 1.6f), 1.6f, 1.05f);
      const Vector3 shotVelocity =
          (goalTarget - origin).GetNormalized(Vector3(-side, 0.0f, 0.0f)) * 32.0f;
      match->SetBallRetainer(nullptr);
      match->GetBall()->SetPosition(origin);
      match->GetBall()->SetMomentum(shotVelocity);
      shotAnchorSent = true;
      match->SpamMessage("SV anchor: Vlahovic -> GOAL", 2200);
    }

    // Once both factual anchors have been emitted, relinquish the last external
    // controller too. The replay page keeps following the ball and its existing
    // hard-outcome safety net remains available if the keeper/physics diverge.
    if (shotAnchorSent && elapsed_ms >= 5200)
      passer->SetExternalController(nullptr);
  }

  void RequestCommand(PlayerCommandQueue& commandQueue) override {
    // Pure passthrough: player behaviour always comes from native Eliza AI.
    if (fallbackController)
      fallbackController->RequestCommand(commandQueue);
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
    passAnchorSent = false;
    shotAnchorSent = false;
    lastDirection = Vector3(1.0f, 0.0f, 0.0f);
    lastVelocity = idleVelocity;
  }

private:
  SoccerverseGuideRole guideRole;
  Player* sourceTarget;
  unsigned long directorStart_ms;
  bool passAnchorSent;
  bool shotAnchorSent;
  Vector3 lastDirection;
  float lastVelocity;
};

#endif  // _HPP_MENU_INGAME_SOCCERVERSEGUIDEDCONTROLLER

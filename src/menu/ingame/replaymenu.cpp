#include "replaymenu.hpp"

#include <algorithm>
#include <cmath>

#include "../../hid/gamepad.hpp"
#include "../../hid/keyboard.hpp"
#include "framework/scheduler.hpp"
#include "main.hpp"
#include "managers/environmentmanager.hpp"
#include "utils/gui2/widgets/caption.hpp"
#include "utils/gui2/widgets/frame.hpp"
#include "utils/localization.hpp"

using namespace blunted;

namespace {

bool HasPlayerRole(Player* player, e_PlayerRole role) {
  if (!player || !player->GetPlayerData())
    return false;
  const std::vector<e_PlayerRole>& roles = player->GetPlayerData()->GetRoles();
  return std::find(roles.begin(), roles.end(), role) != roles.end();
}

}  // namespace

ReplayPage::ReplayPage(Gui2WindowManager* windowManager, const Gui2PageData& pageData)
    : Gui2Page(windowManager, pageData) {
  match = GetGameTask()->GetMatch();

  soccerverseDemo =
      pageData.properties && pageData.properties->GetBool("soccerverse_demo_387016", false);
  soccerverseDemoFinished = false;
  soccerverseOutcomeNudgeApplied = false;
  soccerverseGoalObserved = false;
  soccerversePassAnchorSent = false;
  soccerverseShotAnchorSent = false;
  soccerverseDemoStart_ms = 0;
  soccerverseDemoElapsed_ms = 0;
  soccerverseDemoDuration_ms = 4200;
  soccerverseStartAwayScore = 0;
  soccerversePasser = nullptr;
  soccerverseRunner = nullptr;
  soccerverseHomeGoalkeeper = nullptr;

  this->SetFocus();
  this->Show();

  signed long tmp =
      match->GetActualTime_ms() - match->GetReplaySize_ms();  // must be signed for negative numbers
  minTime_ms = std::max((signed long)10, tmp);
  signed long tmp2 = static_cast<signed long>(match->GetActualTime_ms()) - 10;
  maxTime_ms = static_cast<unsigned long>(std::max(10L, tmp2));
  actualTime_ms = clamp(maxTime_ms - 3000, minTime_ms, maxTime_ms);
  replayCamCount = match->GetReplayCamCount();

  cam = 0;
  modifierValue = 0.0f;
  autoRun = false;
  slowMotion = false;
  stayInReplay = true;
  closeWhenAutorunCompletes = false;

  Gui2Frame* header = new Gui2Frame(windowManager, "frame_replay_header", 29, 2, 42, 7, true);
  this->AddView(header);
  header->Show();
  const std::string titleText =
      soccerverseDemo ? "Soccerverse #387016 - Guided Sim v2"
                      : Localization::GetInstance().Translate("ingame_replay_title");
  Gui2Caption* title =
      new Gui2Caption(windowManager, "caption_replay_title", 2, 2, 38, 3, titleText);
  title->SetPosition(21.0f - title->GetTextWidthPercent() * 0.5f, 2.0f);
  header->AddView(title);
  title->Show();

  Gui2Frame* footer = new Gui2Frame(windowManager, "frame_replay_footer", 10, 88, 80, 10, true);
  this->AddView(footer);
  footer->Show();
  const std::string helpText =
      soccerverseDemo
          ? "LIVE GUIDED: AI e 22 giocatori attivi | passaggio corto = camera | passaggio alto = riavvia"
          : Localization::GetInstance().Translate("ingame_replay_help");
  Gui2Caption* help =
      new Gui2Caption(windowManager, "caption_replay_help", 2, 1.2f, 76, 2.5f, helpText);
  help->SetPosition(40.0f - help->GetTextWidthPercent() * 0.5f, 1.2f);
  footer->AddView(help);
  help->Show();

  timeLabel = new Gui2Caption(windowManager, "caption_replay_time", 2, 5.0f, 76, 3, "");
  footer->AddView(timeLabel);
  timeLabel->Show();

  sig_OnClose.connect([this](...) { OnClose(); });

  if (soccerverseDemo) {
    SetupSoccerverseGuidedSimulation();
  } else {
    match->SetAutoUpdateIngameCamera(false);

    match->replayState.Lock();
    match->replayState->viewTime_ms = actualTime_ms;
    match->replayState->cam = cam;
    match->replayState->modifierValue = 0.0f;
    match->replayState->dirty = true;
    match->replayState.Unlock();
  }

  UpdateTimeLabel();
}

ReplayPage::~ReplayPage() {}

Player* ReplayPage::FindGoalkeeper(const std::vector<Player*>& players) const {
  for (Player* player : players) {
    if (HasPlayerRole(player, e_PlayerRole_GK))
      return player;
  }
  return nullptr;
}

Player* ReplayPage::FindRoleCandidate(const std::vector<Player*>& players,
                                      e_PlayerRole primaryRole, e_PlayerRole secondaryRole,
                                      Player* exclude) const {
  for (Player* player : players) {
    if (player != exclude && !HasPlayerRole(player, e_PlayerRole_GK) &&
        HasPlayerRole(player, primaryRole))
      return player;
  }
  for (Player* player : players) {
    if (player != exclude && !HasPlayerRole(player, e_PlayerRole_GK) &&
        HasPlayerRole(player, secondaryRole))
      return player;
  }
  for (Player* player : players) {
    if (player != exclude && !HasPlayerRole(player, e_PlayerRole_GK))
      return player;
  }
  return nullptr;
}

Vector3 ReplayPage::Get442StartPosition(Player* player, int slot) const {
  if (!player)
    return Vector3(0.0f);
  const float side = static_cast<float>(player->GetTeam()->GetSide());
  switch (slot) {
    case 0:
      return Vector3(side * 51.0f, 0.0f, 0.0f);
    case 1:
      return Vector3(side * 35.0f, -24.0f, 0.0f);
    case 2:
      return Vector3(side * 38.0f, -8.0f, 0.0f);
    case 3:
      return Vector3(side * 38.0f, 8.0f, 0.0f);
    case 4:
      return Vector3(side * 35.0f, 24.0f, 0.0f);
    case 5:
      return Vector3(side * 9.0f, -25.0f, 0.0f);
    case 6:
      return Vector3(side * 10.0f, -8.0f, 0.0f);
    case 7:
      return Vector3(side * 10.0f, 8.0f, 0.0f);
    case 8:
      return Vector3(side * 9.0f, 25.0f, 0.0f);
    case 9:
      return Vector3(side * -20.0f, -8.0f, 0.0f);
    case 10:
      return Vector3(side * -20.0f, 8.0f, 0.0f);
    default:
      return player->GetPosition();
  }
}

void ReplayPage::PlaceTeamIn442(const std::vector<Player*>& players, std::vector<int>& assignedSlots) {
  assignedSlots.assign(players.size(), -1);
  std::vector<bool> used(players.size(), false);

  Player* goalkeeper = FindGoalkeeper(players);
  for (unsigned int i = 0; i < players.size(); ++i) {
    if (players[i] == goalkeeper) {
      assignedSlots[i] = 0;
      used[i] = true;
      break;
    }
  }

  struct SlotRole {
    int slot;
    e_PlayerRole primary;
    e_PlayerRole secondary;
  };
  const SlotRole desired[] = {
      {1, e_PlayerRole_RB, e_PlayerRole_CB}, {2, e_PlayerRole_CB, e_PlayerRole_RB},
      {3, e_PlayerRole_CB, e_PlayerRole_LB}, {4, e_PlayerRole_LB, e_PlayerRole_CB},
      {5, e_PlayerRole_RM, e_PlayerRole_CM}, {6, e_PlayerRole_CM, e_PlayerRole_DM},
      {7, e_PlayerRole_DM, e_PlayerRole_CM}, {8, e_PlayerRole_LM, e_PlayerRole_CM},
      {9, e_PlayerRole_CF, e_PlayerRole_AM}, {10, e_PlayerRole_CF, e_PlayerRole_AM},
  };

  for (const SlotRole& wanted : desired) {
    int selected = -1;
    for (unsigned int i = 0; i < players.size(); ++i) {
      if (!used[i] && HasPlayerRole(players[i], wanted.primary)) {
        selected = static_cast<int>(i);
        break;
      }
    }
    if (selected < 0) {
      for (unsigned int i = 0; i < players.size(); ++i) {
        if (!used[i] && HasPlayerRole(players[i], wanted.secondary)) {
          selected = static_cast<int>(i);
          break;
        }
      }
    }
    if (selected < 0) {
      for (unsigned int i = 0; i < players.size(); ++i) {
        if (!used[i]) {
          selected = static_cast<int>(i);
          break;
        }
      }
    }
    if (selected >= 0) {
      assignedSlots[selected] = wanted.slot;
      used[selected] = true;
    }
  }

  for (unsigned int i = 0; i < players.size(); ++i) {
    if (assignedSlots[i] < 0)
      assignedSlots[i] = static_cast<int>(std::min<unsigned int>(10, i));
    players[i]->ResetPosition(Get442StartPosition(players[i], assignedSlots[i]), Vector3(0.0f));
  }
}

void ReplayPage::SetupSoccerverseGuidedSimulation() {
  soccerverseDemoFinished = false;
  soccerverseOutcomeNudgeApplied = false;
  soccerverseGoalObserved = false;
  soccerversePassAnchorSent = false;
  soccerverseShotAnchorSent = false;
  soccerverseDemoElapsed_ms = 0;
  soccerverseAllPlayers.clear();
  soccerversePreviousExternalControllers.clear();
  soccerverseGuideControllers.clear();

  std::vector<Player*> homePlayers;
  std::vector<Player*> awayPlayers;
  match->GetActiveTeamPlayers(0, homePlayers);
  match->GetActiveTeamPlayers(1, awayPlayers);

  soccerverseHomeGoalkeeper = FindGoalkeeper(homePlayers);
  soccerverseRunner = FindRoleCandidate(awayPlayers, e_PlayerRole_CF, e_PlayerRole_AM);
  soccerversePasser =
      FindRoleCandidate(awayPlayers, e_PlayerRole_DM, e_PlayerRole_CM, soccerverseRunner);

  if (homePlayers.size() < 11 || awayPlayers.size() < 11 || !soccerverseHomeGoalkeeper ||
      !soccerverseRunner || !soccerversePasser) {
    soccerverseDemoFinished = true;
    match->SpamMessage("Guided Sim v2: impossibile costruire due XI + attori sorgente.", 5000);
    return;
  }

  std::vector<int> homeSlots;
  std::vector<int> awaySlots;
  PlaceTeamIn442(homePlayers, homeSlots);
  PlaceTeamIn442(awayPlayers, awaySlots);

  // Keep the 4-4-2 block alive, but reconstruct this event in the attacking
  // third instead of starting Kalulu near midfield. These are reconstructed
  // spatial positions, never claimed as historical Soccerverse tracking.
  const float awaySide = static_cast<float>(soccerversePasser->GetTeam()->GetSide());
  const Vector3 goalCentre(-awaySide * pitchHalfW, 0.0f, 0.0f);
  const Vector3 passerStart(-awaySide * 18.0f, 5.5f, 0.0f);
  const Vector3 runnerStart(-awaySide * 31.0f, 1.5f, 0.0f);
  soccerversePasser->ResetPosition(passerStart, runnerStart);
  soccerverseRunner->ResetPosition(runnerStart, goalCentre);
  soccerverseHomeGoalkeeper->ResetPosition(Vector3(-awaySide * 50.4f, 0.0f, 0.0f), runnerStart);

  soccerverseAllPlayers.insert(soccerverseAllPlayers.end(), homePlayers.begin(), homePlayers.end());
  soccerverseAllPlayers.insert(soccerverseAllPlayers.end(), awayPlayers.begin(), awayPlayers.end());

  // Stability invariant: the Director never owns player controllers.
  for (Player* player : soccerverseAllPlayers) {
    soccerversePreviousExternalControllers.push_back(player->GetExternalController());
    player->SetExternalController(nullptr);
  }

  // Clear stale possession/AI/ball state from the live match before starting
  // the isolated golden action. Player positions stay reconstructed above.
  match->ResetSituation(passerStart);
  soccerversePasser->ResetPosition(passerStart, runnerStart);
  soccerverseRunner->ResetPosition(runnerStart, goalCentre);
  soccerverseHomeGoalkeeper->ResetPosition(Vector3(-awaySide * 50.4f, 0.0f, 0.0f), runnerStart);

  const Vector3 ballStart = passerStart + Vector3(-awaySide * 0.45f, 0.0f, 0.11f);
  match->GetBall()->SetPosition(ballStart);
  match->GetBall()->SetMomentum(Vector3(0.0f));
  match->SetBallRetainer(soccerversePasser);
  match->SetGoalScored(false);
  match->StopSetPiece();
  match->StartPlay();
  match->SetAutoUpdateIngameCamera(false);

  soccerverseDemoStart_ms = EnvironmentManager::GetInstance().GetTime_ms();
  soccerverseStartAwayScore = match->GetScore(1);
  match->Pause(false);

  match->SpamMessage("SV 68': Kalulu -> Vlahovic -> GOAL | single action", 2200);
}

void ReplayPage::StopSoccerverseGuidedSimulation() {
  match->SetBallRetainer(nullptr);

  const unsigned int restoreCount = std::min(soccerverseAllPlayers.size(),
                                              soccerversePreviousExternalControllers.size());
  for (unsigned int i = 0; i < restoreCount; ++i)
    soccerverseAllPlayers[i]->SetExternalController(soccerversePreviousExternalControllers[i]);

  soccerverseGuideControllers.clear();
  soccerversePreviousExternalControllers.clear();
  soccerverseAllPlayers.clear();
}

void ReplayPage::ResetSoccerverseGuidedSimulation() {
  match->Pause(true);
  StopSoccerverseGuidedSimulation();
  SetupSoccerverseGuidedSimulation();
}

void ReplayPage::ProcessSoccerverseGuidedSimulation() {
  if (soccerverseDemoFinished || !soccerversePasser || !soccerverseRunner)
    return;

  const unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();
  soccerverseDemoElapsed_ms =
      now_ms >= soccerverseDemoStart_ms ? now_ms - soccerverseDemoStart_ms : 0;

  if (match->GetScore(1) > soccerverseStartAwayScore ||
      (match->IsGoalScored() && match->GetLastGoalTeamID() == 1)) {
    soccerverseGoalObserved = true;
  }

  const float side = static_cast<float>(soccerversePasser->GetTeam()->GetSide());

  // Start the factual action almost immediately. The native AI remains alive,
  // but it no longer gets 1.2 seconds to invent a different opening possession.
  if (!soccerversePassAnchorSent && soccerverseDemoElapsed_ms >= 250) {
    const Vector3 origin =
        soccerversePasser->GetPosition() + Vector3(-side * 0.48f, 0.0f, 0.13f);
    const Vector3 target =
        soccerverseRunner->GetPosition() + Vector3(-side * 0.75f, 0.0f, 0.10f);
    const Vector3 passVelocity =
        (target - origin).GetNormalized(Vector3(-side, 0.0f, 0.0f)) * 17.0f;
    match->SetBallRetainer(nullptr);
    match->GetTeam(soccerversePasser->GetTeamID())
        ->SetLastTouchPlayer(soccerversePasser, e_TouchType_Intentional_Kicked);
    match->GetBall()->SetPosition(origin);
    match->GetBall()->SetMomentum(passVelocity);
    soccerversePassAnchorSent = true;
    match->SpamMessage("SV: Kalulu -> Vlahovic", 1200);
  }

  // One reconciliation at Vlahovic's feet, then one hard factual shot. Aim
  // across the goalkeeper and inside the far corner so the native goal detector
  // sees an actual line crossing rather than an artificial score mutation.
  if (!soccerverseShotAnchorSent && soccerverseDemoElapsed_ms >= 1350) {
    const Vector3 origin =
        soccerverseRunner->GetPosition() + Vector3(-side * 0.50f, 0.0f, 0.14f);
    const float farCornerY = origin.coords[1] >= 0.0f ? -2.85f : 2.85f;
    const Vector3 goalTarget(-side * (pitchHalfW + 1.4f), farCornerY, 1.35f);
    const Vector3 shotVelocity =
        (goalTarget - origin).GetNormalized(Vector3(-side, 0.0f, 0.0f)) * 40.0f;
    match->SetBallRetainer(nullptr);
    match->GetTeam(soccerverseRunner->GetTeamID())
        ->SetLastTouchPlayer(soccerverseRunner, e_TouchType_Intentional_Kicked);
    match->GetBall()->SetPosition(origin);
    match->GetBall()->SetMomentum(shotVelocity);
    soccerverseShotAnchorSent = true;
    match->SpamMessage("SV: Vlahovic -> GOAL", 1500);
  }

  // Safety net only if the keeper/physics deflect the factual shot. Prefer a
  // continuation from the current ball when it is still close to goal; only
  // reconcile near Vlahovic when the native sim has sent it far away.
  if (!soccerverseGoalObserved && soccerverseShotAnchorSent && !soccerverseOutcomeNudgeApplied &&
      soccerverseDemoElapsed_ms >= 2600) {
    Vector3 origin = match->GetBall()->Predict(0);
    const Vector3 goalMouth(-side * pitchHalfW, 0.0f, 1.0f);
    if (origin.Get2D().GetDistance(goalMouth.Get2D()) > 14.0f) {
      origin = soccerverseRunner->GetPosition() + Vector3(-side * 0.55f, 0.0f, 0.14f);
      match->GetBall()->SetPosition(origin);
    }
    const float cornerY = origin.coords[1] >= 0.0f ? -2.6f : 2.6f;
    const Vector3 goalTarget(-side * (pitchHalfW + 1.5f), cornerY, 1.15f);
    const Vector3 shotVelocity =
        (goalTarget - origin).GetNormalized(Vector3(-side, 0.0f, 0.0f)) * 42.0f;
    match->GetTeam(soccerverseRunner->GetTeamID())
        ->SetLastTouchPlayer(soccerverseRunner, e_TouchType_Intentional_Kicked);
    match->GetBall()->SetMomentum(shotVelocity);
    soccerverseOutcomeNudgeApplied = true;
  }

  const Vector3 ballPosition = match->GetBall()->Predict(0);
  match->SetReplayCamera(cam, ballPosition, modifierValue);

  // Once the factual goal exists, do not let the referee/AI create the next
  // restart sequence. Hold the goal briefly, then freeze the golden clip.
  if (soccerverseGoalObserved && soccerverseShotAnchorSent && soccerverseDemoElapsed_ms >= 2300) {
    soccerverseDemoFinished = true;
    match->Pause(true);
    match->SpamMessage("SV 68': Kalulu -> Vlahovic -> GOAL | GOAL OK", 5000);
  } else if (soccerverseDemoElapsed_ms >= soccerverseDemoDuration_ms) {
    soccerverseDemoElapsed_ms = soccerverseDemoDuration_ms;
    soccerverseDemoFinished = true;
    match->Pause(true);
  }

  UpdateTimeLabel();
}

void ReplayPage::OnClose() {
  if (soccerverseDemo) {
    StopSoccerverseGuidedSimulation();
  } else {
    match->replayState.Lock();
    match->replayState->viewTime_ms = maxTime_ms;
    match->replayState->cam = cam;
    match->replayState->modifierValue = 0.0f;
    match->replayState->dirty = true;
    match->replayState.Unlock();
  }

  GetScheduler()->ResetTaskSequenceTime("game");
  match->SetAutoUpdateIngameCamera(true);

  if (stayInReplay)
    match->Pause(false);
}

void ReplayPage::Autorun(int replayHistoryOffset_ms, bool stayInReplay) {
  autoRun = true;
  closeWhenAutorunCompletes = true;
  cam = 1;
  modifierValue = 0.0;
  signed long tmp = maxTime_ms - replayHistoryOffset_ms;
  actualTime_ms = clamp(tmp, minTime_ms, maxTime_ms);
  this->stayInReplay = stayInReplay;
}

void ReplayPage::UpdateTimeLabel() {
  if (soccerverseDemo) {
    std::string label = "SV #387016 | 68' | Kalulu > Vlahovic > GOAL | " +
                        int_to_str(soccerverseDemoElapsed_ms / 1000) + "s / " +
                        int_to_str(soccerverseDemoDuration_ms / 1000) + "s";
    if (soccerverseGoalObserved)
      label += " | GOAL OK";
    else if (soccerverseOutcomeNudgeApplied)
      label += " | outcome constraint";
    if (soccerverseDemoFinished)
      label += " | FINE";
    timeLabel->SetCaption(label);
    timeLabel->SetPosition(40.0f - timeLabel->GetTextWidthPercent() * 0.5f, 5.0f);
    return;
  }

  unsigned long replaySize_ms = maxTime_ms - minTime_ms;
  unsigned long elapsed_ms = actualTime_ms - minTime_ms;
  float positionPct = (replaySize_ms > 0) ? (elapsed_ms * 100.0f / replaySize_ms) : 0.0f;
  unsigned long secsAgo = (maxTime_ms - actualTime_ms) / 1000;
  std::string label = std::string(slowMotion ? "[0.5x] " : "") + int_to_str(elapsed_ms / 1000) +
                      "s / " + int_to_str(replaySize_ms / 1000) + "s  (" +
                      int_to_str(static_cast<int>(round(positionPct))) + "%)  -" +
                      int_to_str(secsAgo) + "s";
  timeLabel->SetCaption(label);
  timeLabel->SetPosition(35.0f - timeLabel->GetTextWidthPercent() * 0.5f, 4.5f);
}

void ReplayPage::Process() {
  if (soccerverseDemo) {
    ProcessSoccerverseGuidedSimulation();
    return;
  }

  if (autoRun) {
    Vector3 direction;
    direction.coords[0] = slowMotion ? 0.25f : 0.5f;
    ProcessInput(direction, false, false, false);
  }
}

void ReplayPage::ProcessKeyboardEvent(KeyboardEvent* event) {
  const std::vector<IHIDevice*>& controllers = GetControllers();
  HIDKeyboard* keyboard = nullptr;
  for (IHIDevice* c : controllers) {
    if (c && c->GetDeviceType() == e_HIDeviceType_Keyboard) {
      keyboard = static_cast<HIDKeyboard*>(c);
      break;
    }
  }
  if (!keyboard)
    return;

  bool button1 = false;
  bool button2 = false;
  bool slowMo = false;
  if (event->GetKeyOnce(keyboard->GetFunctionMapping(e_ButtonFunction_ShortPass)))
    button1 = true;
  if (event->GetKeyOnce(keyboard->GetFunctionMapping(e_ButtonFunction_HighPass)))
    button2 = true;
  if (event->GetKeyContinuous(keyboard->GetFunctionMapping(e_ButtonFunction_Sprint)))
    slowMo = true;

  Vector3 direction;
  if (event->GetKeyContinuous(keyboard->GetFunctionMapping(e_ButtonFunction_Left)))
    direction.coords[0] += -0.5f;
  if (event->GetKeyContinuous(keyboard->GetFunctionMapping(e_ButtonFunction_Right)))
    direction.coords[0] += 0.5f;
  if (event->GetKeyContinuous(keyboard->GetFunctionMapping(e_ButtonFunction_Up)))
    direction.coords[1] += -0.5f;
  if (event->GetKeyContinuous(keyboard->GetFunctionMapping(e_ButtonFunction_Down)))
    direction.coords[1] += 0.5f;

  ProcessInput(direction, button1, button2, slowMo);
}

void ReplayPage::ProcessJoystickEvent(JoystickEvent* event) {
  const std::vector<IHIDevice*>& controllers = GetControllers();

  HIDGamepad* gamepad = nullptr;
  for (IHIDevice* c : controllers) {
    if (c && c->GetDeviceType() == e_HIDeviceType_Gamepad) {
      gamepad = static_cast<HIDGamepad*>(c);
      break;
    }
  }
  if (!gamepad)
    return;

  bool button1 =
      event->GetButton(0, gamepad->GetControllerMapping(
                              gamepad->GetFunctionMapping(e_ButtonFunction_LongPass))) ||
      event->GetButton(0, gamepad->GetControllerMapping(
                              gamepad->GetFunctionMapping(e_ButtonFunction_ShortPass)));
  bool button2 =
      event->GetButton(0, gamepad->GetControllerMapping(
                              gamepad->GetFunctionMapping(e_ButtonFunction_HighPass))) ||
      event->GetButton(0, gamepad->GetControllerMapping(
                              gamepad->GetFunctionMapping(e_ButtonFunction_Shot)));
  bool slowMo = event->GetButton(
      0, gamepad->GetControllerMapping(gamepad->GetFunctionMapping(e_ButtonFunction_Sprint)));

  Vector3 direction;
  direction.coords[0] = event->GetAxis(0, 0);
  direction.coords[1] = event->GetAxis(0, 1);

  float deadzone = 0.2f;
  if (fabs(direction.coords[0]) < deadzone) {
    direction.coords[0] = 0.0f;
  } else {
    direction.coords[0] =
        pow((fabs(direction.coords[0]) - deadzone) * (1.0f / (1.0f - deadzone)), 2.0f) *
        signSide(direction.coords[0]);
  }
  deadzone = 0.4f;
  if (fabs(direction.coords[1]) < deadzone) {
    direction.coords[1] = 0.0f;
  } else {
    direction.coords[1] =
        pow((fabs(direction.coords[1]) - deadzone) * (1.0f / (1.0f - deadzone)), 4.0f) *
        signSide(direction.coords[1]);
  }

  ProcessInput(direction, button1, button2, slowMo);
}

void ReplayPage::ProcessInput(const Vector3& direction, bool button1, bool button2,
                              bool slowMoInput) {
  if (soccerverseDemo) {
    if (button1) {
      cam++;
      if (cam == replayCamCount)
        cam = 0;
    }
    if (button2)
      ResetSoccerverseGuidedSimulation();
    if (direction.coords[1] != 0.0f)
      modifierValue = clamp(modifierValue + direction.coords[1] * 0.05f, -1.0f, 1.0f);
    return;
  }

  slowMotion = slowMoInput;

  if (button2 && autoRun == false) {
    actualTime_ms = minTime_ms;
    autoRun = true;
    closeWhenAutorunCompletes = false;
  } else if (button2) {
    autoRun = false;
    closeWhenAutorunCompletes = false;
  }
  if (button1 && autoRun == true) {
    autoRun = false;
    closeWhenAutorunCompletes = false;
  } else if (button1) {
    cam++;
    if (cam == replayCamCount)
      cam = 0;
  }

  if (!autoRun)
    modifierValue += direction.coords[1] * 0.05f;

  if (cam == 2) {
    if (modifierValue < -1.0f)
      modifierValue += 2.0f;
    if (modifierValue > 1.0f)
      modifierValue -= 2.0f;
  } else {
    modifierValue = clamp(modifierValue, -1.0f, 1.0f);
  }

  float speedMultiplier = slowMotion ? 0.5f : 1.0f;
  float timeMovement = direction.coords[0] * 2.0f * speedMultiplier;
  actualTime_ms += int(round(timeMovement * 10.0f));

  if (autoRun && actualTime_ms >= (signed int)maxTime_ms) {
    autoRun = false;
    if (closeWhenAutorunCompletes) {
      closeWhenAutorunCompletes = false;
      GoBack();
      return;
    }
  }

  actualTime_ms = clamp(actualTime_ms, minTime_ms, maxTime_ms);

  UpdateTimeLabel();

  unsigned long resultTime = actualTime_ms;

  match->replayState.Lock();
  match->replayState->viewTime_ms = resultTime;
  match->replayState->cam = cam;
  match->replayState->modifierValue = modifierValue;
  match->replayState->dirty = true;
  match->replayState.Unlock();
}

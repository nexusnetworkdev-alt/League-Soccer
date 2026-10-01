#include "replaymenu.hpp"

#include "../../hid/gamepad.hpp"
#include "../../hid/keyboard.hpp"
#include "framework/scheduler.hpp"
#include "main.hpp"
#include "managers/environmentmanager.hpp"
#include "utils/gui2/widgets/caption.hpp"
#include "utils/gui2/widgets/frame.hpp"
#include "utils/localization.hpp"

using namespace blunted;

ReplayPage::ReplayPage(Gui2WindowManager* windowManager, const Gui2PageData& pageData)
    : Gui2Page(windowManager, pageData) {
  match = GetGameTask()->GetMatch();

  soccerverseDemo = pageData.properties && pageData.properties->GetBool("soccerverse_demo_387016", false);
  soccerverseDemoFinished = false;
  soccerverseDemoStart_ms = 0;
  soccerverseDemoElapsed_ms = 0;
  soccerverseDemoDuration_ms = 8500;
  soccerverseGoalkeeper = nullptr;
  soccerverseOriginalBallPosition = Vector3(0);
  soccerverseOriginalBallMomentum = Vector3(0);

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

  Gui2Frame* header = new Gui2Frame(windowManager, "frame_replay_header", 32, 2, 36, 7, true);
  this->AddView(header);
  header->Show();
  const std::string titleText = soccerverseDemo
                                    ? "Soccerverse #387016 - 3D PoC"
                                    : Localization::GetInstance().Translate("ingame_replay_title");
  Gui2Caption* title =
      new Gui2Caption(windowManager, "caption_replay_title", 2, 2, 32, 3, titleText);
  title->SetPosition(18.0f - title->GetTextWidthPercent() * 0.5f, 2.0f);
  header->AddView(title);
  title->Show();

  Gui2Frame* footer = new Gui2Frame(windowManager, "frame_replay_footer", 15, 89, 70, 9, true);
  this->AddView(footer);
  footer->Show();
  const std::string helpText = soccerverseDemo
                                   ? "PoC: eventi Soccerverse reali, geometria 3D ricostruita. Passaggio corto = camera; passaggio alto = riavvia."
                                   : Localization::GetInstance().Translate("ingame_replay_help");
  Gui2Caption* help =
      new Gui2Caption(windowManager, "caption_replay_help", 2, 1.2f, 66, 2.5f, helpText);
  help->SetPosition(35.0f - help->GetTextWidthPercent() * 0.5f, 1.2f);
  footer->AddView(help);
  help->Show();

  timeLabel = new Gui2Caption(windowManager, "caption_replay_time", 2, 4.5f, 66, 3, "");
  footer->AddView(timeLabel);
  timeLabel->Show();

  sig_OnClose.connect([this](...) { OnClose(); });

  if (soccerverseDemo) {
    // Keep the normal match paused, but render a dedicated reconstruction directly with the
    // existing League-Soccer 3D scene, player models and camera system.
    match->SetAutoUpdateIngameCamera(false);

    match->GetActiveTeamPlayers(0, soccerverseAllPlayers);
    match->GetActiveTeamPlayers(1, soccerverseAllPlayers);
    for (Player* player : soccerverseAllPlayers) {
      soccerverseOriginalPositions.push_back(player->GetHumanoidNode()->GetPosition());
    }

    std::vector<Player*> attackingPlayers;
    match->GetActiveTeamPlayers(0, attackingPlayers);
    for (Player* player : attackingPlayers) {
      bool goalkeeper = false;
      const std::vector<e_PlayerRole>& roles = player->GetPlayerData()->GetRoles();
      for (e_PlayerRole role : roles) {
        if (role == e_PlayerRole_GK) {
          goalkeeper = true;
          break;
        }
      }
      if (!goalkeeper && soccerverseActors.size() < 7)
        soccerverseActors.push_back(player);
    }

    std::vector<Player*> defendingPlayers;
    match->GetActiveTeamPlayers(1, defendingPlayers);
    for (Player* player : defendingPlayers) {
      const std::vector<e_PlayerRole>& roles = player->GetPlayerData()->GetRoles();
      for (e_PlayerRole role : roles) {
        if (role == e_PlayerRole_GK) {
          soccerverseGoalkeeper = player;
          break;
        }
      }
      if (soccerverseGoalkeeper)
        break;
    }

    soccerverseOriginalBallPosition = match->GetBall()->GetBallGeom()->GetPosition();
    soccerverseOriginalBallMomentum = match->GetBall()->GetMovement();
    soccerverseDemoStart_ms = EnvironmentManager::GetInstance().GetTime_ms();

    if (soccerverseActors.size() < 7 || !soccerverseGoalkeeper) {
      soccerverseDemoFinished = true;
      match->SpamMessage("Soccerverse PoC: impossibile mappare 7 giocatori + portiere.", 5000);
    } else {
      ProcessSoccerverseDemo();
    }
  } else {
    match->SetAutoUpdateIngameCamera(false);

    match->replayState.Lock();
    match->replayState->viewTime_ms = actualTime_ms;  // minTime_ms;
    match->replayState->cam = cam;
    match->replayState->modifierValue = 0.0f;
    match->replayState->dirty = true;
    match->replayState.Unlock();
  }

  UpdateTimeLabel();
}

ReplayPage::~ReplayPage() {}

void ReplayPage::OnClose() {
  if (soccerverseDemo) {
    // Restore the live match visuals exactly where they were before entering the PoC.
    unsigned int restoreCount =
        std::min(soccerverseAllPlayers.size(), soccerverseOriginalPositions.size());
    for (unsigned int i = 0; i < restoreCount; ++i) {
      Player* player = soccerverseAllPlayers[i];
      player->GetHumanoidNode()->SetPosition(soccerverseOriginalPositions[i], false);
      player->GetHumanoidNode()->RecursiveUpdateSpatialData(e_SpatialDataType_Both);
      player->UpdateFullbodyNodes();
    }
    match->GetBall()->GetBallGeom()->SetPosition(soccerverseOriginalBallPosition, false);
    match->GetDynamicNode()->RecursiveUpdateSpatialData(e_SpatialDataType_Both);
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
    match->Pause(false);  // todo: handle gracefully instead of using stayInReplay :p only unpause
                          // when started from gamepage instead of ingame page
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

Vector3 ReplayPage::GetSoccerverseActorPosition(int actorIndex, unsigned long elapsed_ms) const {
  const Vector3 balogun(-30.0f, -8.0f, 0.0f);
  const Vector3 jensen(-21.0f, -16.0f, 0.0f);
  const Vector3 nygren(-11.0f, -9.0f, 0.0f);
  const Vector3 ambros(-1.0f, -4.0f, 0.0f);
  const Vector3 ramirezFirst(10.0f, 8.0f, 0.0f);
  const Vector3 turay(21.0f, 14.0f, 0.0f);
  const Vector3 garcia(31.0f, 6.0f, 0.0f);
  const Vector3 ramirezShot(40.0f, 2.0f, 0.0f);
  const Vector3 krahlStart(53.0f, 0.0f, 0.0f);
  const Vector3 krahlSave(52.2f, 0.4f, 0.0f);

  auto smooth01 = [](float value) {
    value = clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
  };

  switch (actorIndex) {
    case 0:
      return balogun;
    case 1:
      return jensen;
    case 2:
      return nygren;
    case 3:
      return ambros;
    case 4: {
      if (elapsed_ms <= 3600)
        return ramirezFirst;
      if (elapsed_ms >= 6300)
        return ramirezShot;
      float bias = smooth01(static_cast<float>(elapsed_ms - 3600) / 2700.0f);
      return ramirezFirst * (1.0f - bias) + ramirezShot * bias;
    }
    case 5:
      return turay;
    case 6:
      return garcia;
    case 7: {
      if (elapsed_ms <= 7200)
        return krahlStart;
      if (elapsed_ms >= 8000)
        return krahlSave;
      float bias = smooth01(static_cast<float>(elapsed_ms - 7200) / 800.0f);
      return krahlStart * (1.0f - bias) + krahlSave * bias;
    }
    default:
      return Vector3(0);
  }
}

Vector3 ReplayPage::GetSoccerverseBallPosition(unsigned long elapsed_ms) const {
  struct BallKey {
    unsigned long time_ms;
    Vector3 position;
  };

  const BallKey keys[] = {
      {0, GetSoccerverseActorPosition(0, elapsed_ms) + Vector3(0, 0, 0.11f)},
      {900, GetSoccerverseActorPosition(1, elapsed_ms) + Vector3(0, 0, 0.11f)},
      {1800, GetSoccerverseActorPosition(2, elapsed_ms) + Vector3(0, 0, 0.11f)},
      {2700, GetSoccerverseActorPosition(3, elapsed_ms) + Vector3(0, 0, 0.11f)},
      {3600, Vector3(10.0f, 8.0f, 0.11f)},
      {4500, GetSoccerverseActorPosition(5, elapsed_ms) + Vector3(0, 0, 0.11f)},
      {5400, GetSoccerverseActorPosition(6, elapsed_ms) + Vector3(0, 0, 0.11f)},
      {6300, Vector3(40.0f, 2.0f, 0.11f)},
      {7200, Vector3(40.4f, 2.0f, 0.11f)},
      {8000, GetSoccerverseActorPosition(7, elapsed_ms) + Vector3(0, 0, 1.05f)},
      {8500, GetSoccerverseActorPosition(7, elapsed_ms) + Vector3(-0.15f, 0, 0.95f)},
  };
  const unsigned int keyCount = sizeof(keys) / sizeof(keys[0]);

  if (elapsed_ms <= keys[0].time_ms)
    return keys[0].position;
  if (elapsed_ms >= keys[keyCount - 1].time_ms)
    return keys[keyCount - 1].position;

  for (unsigned int i = 1; i < keyCount; ++i) {
    if (elapsed_ms <= keys[i].time_ms) {
      const BallKey& a = keys[i - 1];
      const BallKey& b = keys[i];
      float bias = static_cast<float>(elapsed_ms - a.time_ms) /
                   static_cast<float>(b.time_ms - a.time_ms);
      bias = clamp(bias, 0.0f, 1.0f);
      bias = bias * bias * (3.0f - 2.0f * bias);
      Vector3 result = a.position * (1.0f - bias) + b.position * bias;
      if (a.time_ms == 7200 && b.time_ms == 8000)
        result.coords[2] += sin(bias * pi) * 1.15f;
      return result;
    }
  }

  return keys[keyCount - 1].position;
}

void ReplayPage::ProcessSoccerverseDemo() {
  if (soccerverseActors.size() < 7 || !soccerverseGoalkeeper)
    return;

  unsigned long now_ms = EnvironmentManager::GetInstance().GetTime_ms();
  unsigned long elapsed_ms = now_ms - soccerverseDemoStart_ms;
  if (elapsed_ms >= soccerverseDemoDuration_ms) {
    elapsed_ms = soccerverseDemoDuration_ms;
    soccerverseDemoFinished = true;
  }
  soccerverseDemoElapsed_ms = elapsed_ms;

  for (unsigned int i = 0; i < soccerverseActors.size(); ++i) {
    Vector3 target = GetSoccerverseActorPosition(static_cast<int>(i), elapsed_ms);
    soccerverseActors[i]->GetHumanoidNode()->SetPosition(target, false);
    soccerverseActors[i]->GetHumanoidNode()->RecursiveUpdateSpatialData(e_SpatialDataType_Both);
    soccerverseActors[i]->UpdateFullbodyNodes();
  }

  Vector3 goalkeeperTarget = GetSoccerverseActorPosition(7, elapsed_ms);
  soccerverseGoalkeeper->GetHumanoidNode()->SetPosition(goalkeeperTarget, false);
  soccerverseGoalkeeper->GetHumanoidNode()->RecursiveUpdateSpatialData(e_SpatialDataType_Both);
  soccerverseGoalkeeper->UpdateFullbodyNodes();

  Vector3 ballPosition = GetSoccerverseBallPosition(elapsed_ms);
  match->GetBall()->GetBallGeom()->SetPosition(ballPosition, false);

  // Use League-Soccer's own replay camera implementation, only changing its target to the staged
  // Soccerverse ball position. Camera type can still be cycled with the normal replay control.
  match->SetReplayCamera(cam, ballPosition, modifierValue);
  match->GetDynamicNode()->RecursiveUpdateSpatialData(e_SpatialDataType_Both);

  UpdateTimeLabel();
}

void ReplayPage::UpdateTimeLabel() {
  if (soccerverseDemo) {
    std::string label = "Soccerverse #387016  |  81:34-82:21  |  " +
                        int_to_str(soccerverseDemoElapsed_ms / 1000) + "s / " +
                        int_to_str(soccerverseDemoDuration_ms / 1000) + "s";
    if (soccerverseDemoFinished)
      label += "  |  FINE CLIP";
    timeLabel->SetCaption(label);
    timeLabel->SetPosition(35.0f - timeLabel->GetTextWidthPercent() * 0.5f, 4.5f);
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
    ProcessSoccerverseDemo();
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
  if (!keyboard) {
    return;
  }

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
  int controllerID = 0;
  const std::vector<IHIDevice*>& controllers = GetControllers();

  // Find the gamepad driving Player 1. Do not assume the keyboard is at index 0
  // and a gamepad at index 1 - with no pad connected that cast would be OOB.
  HIDGamepad* gamepad = nullptr;
  for (IHIDevice* c : controllers) {
    if (c && c->GetDeviceType() == e_HIDeviceType_Gamepad) {
      gamepad = static_cast<HIDGamepad*>(c);
      break;
    }
  }
  if (!gamepad) {
    return;
  }

  bool button1 =
      event->GetButton(0, gamepad->GetControllerMapping(
                              gamepad->GetFunctionMapping(e_ButtonFunction_LongPass))) ||
      event->GetButton(0,
                       gamepad->GetControllerMapping(gamepad->GetFunctionMapping(
                           e_ButtonFunction_ShortPass)));  // need 2 options because maybe the first
                                                           // is set to gui's 'escape' function
  bool button2 =
      event->GetButton(0, gamepad->GetControllerMapping(
                              gamepad->GetFunctionMapping(e_ButtonFunction_HighPass))) ||
      event->GetButton(0, gamepad->GetControllerMapping(gamepad->GetFunctionMapping(
                              e_ButtonFunction_Shot)));  // need 2 options because maybe the first
                                                         // is set to gui's 'escape' function
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
    if (button2) {
      soccerverseDemoStart_ms = EnvironmentManager::GetInstance().GetTime_ms();
      soccerverseDemoElapsed_ms = 0;
      soccerverseDemoFinished = false;
    }
    if (direction.coords[1] != 0.0f)
      modifierValue = clamp(modifierValue + direction.coords[1] * 0.05f, -1.0f, 1.0f);
    return;
  }

  // slow-motion: held sprint button halves playback speed
  slowMotion = slowMoInput;

  // autorun
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

  if (!autoRun) {
    modifierValue += direction.coords[1] * 0.05f;
  }

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

  // feed results to match - replays are effectively replayed there

  match->replayState.Lock();
  match->replayState->viewTime_ms = resultTime;
  match->replayState->cam = cam;
  match->replayState->modifierValue = modifierValue;
  match->replayState->dirty = true;
  match->replayState.Unlock();
}

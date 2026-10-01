#ifndef _HPP_MENU_INGAME_REPLAY
#define _HPP_MENU_INGAME_REPLAY

#include "../../onthepitch/match.hpp"
#include "utils/gui2/page.hpp"
#include "utils/gui2/widgets/caption.hpp"
#include "utils/gui2/windowmanager.hpp"

using namespace blunted;

class ReplayPage : public Gui2Page {
public:
  ReplayPage(Gui2WindowManager* windowManager, const Gui2PageData& pageData);
  virtual ~ReplayPage();

  void OnClose();
  void Autorun(int replayHistoryOffset_ms, bool stayInReplay);

protected:
  Match* match;

  virtual void Process();
  virtual void ProcessKeyboardEvent(KeyboardEvent* event);
  virtual void ProcessJoystickEvent(JoystickEvent* event);
  void ProcessInput(const Vector3& direction, bool button1, bool button2, bool slowMotion);
  void UpdateTimeLabel();
  void ProcessSoccerverseDemo();
  Vector3 GetSoccerverseBallPosition(unsigned long elapsed_ms) const;
  Vector3 GetSoccerverseActorPosition(int actorIndex, unsigned long elapsed_ms) const;

  signed long actualTime_ms;
  unsigned long minTime_ms;
  unsigned long maxTime_ms;

  int cam;
  int replayCamCount;
  float modifierValue;

  bool autoRun;
  bool stayInReplay;
  bool slowMotion;
  bool closeWhenAutorunCompletes;

  bool soccerverseDemo;
  bool soccerverseDemoFinished;
  unsigned long soccerverseDemoStart_ms;
  unsigned long soccerverseDemoElapsed_ms;
  unsigned long soccerverseDemoDuration_ms;
  std::vector<Player*> soccerverseActors;
  std::vector<Player*> soccerverseAllPlayers;
  std::vector<Vector3> soccerverseOriginalPositions;
  Player* soccerverseGoalkeeper;
  Vector3 soccerverseOriginalBallPosition;
  Vector3 soccerverseOriginalBallMomentum;

  Gui2Caption* timeLabel;
};

#endif

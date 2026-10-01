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

  void SetupSoccerverseGuidedSimulation();
  void StopSoccerverseGuidedSimulation();
  void ProcessSoccerverseGuidedSimulation();
  void ResetSoccerverseGuidedSimulation();
  Player* FindGoalkeeper(const std::vector<Player*>& players) const;
  Player* FindRoleCandidate(const std::vector<Player*>& players, e_PlayerRole primaryRole,
                            e_PlayerRole secondaryRole, Player* exclude = nullptr) const;
  void PlaceTeamIn442(const std::vector<Player*>& players, std::vector<int>& assignedSlots);
  Vector3 Get442StartPosition(Player* player, int slot) const;

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
  bool soccerverseOutcomeNudgeApplied;
  bool soccerverseGoalObserved;
  unsigned long soccerverseDemoStart_ms;
  unsigned long soccerverseDemoElapsed_ms;
  unsigned long soccerverseDemoDuration_ms;
  int soccerverseStartAwayScore;
  std::vector<Player*> soccerverseAllPlayers;
  std::vector<IController*> soccerversePreviousExternalControllers;
  std::vector<IController*> soccerverseGuideControllers;
  Player* soccerversePasser;
  Player* soccerverseRunner;
  Player* soccerverseHomeGoalkeeper;

  Gui2Caption* timeLabel;
};

#endif

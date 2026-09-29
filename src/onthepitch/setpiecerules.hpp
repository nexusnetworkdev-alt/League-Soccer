#ifndef _HPP_FOOTBALL_ONTHEPITCH_SETPIECERULES
#define _HPP_FOOTBALL_ONTHEPITCH_SETPIECERULES

enum e_SetPiece {
  e_SetPiece_None,
  e_SetPiece_KickOff,
  e_SetPiece_GoalKick,
  e_SetPiece_FreeKick,
  e_SetPiece_Corner,
  e_SetPiece_ThrowIn,
  e_SetPiece_Penalty,
};
// The exemption applies only to the first touch directly from the restart.
inline bool IsOffsideExemptRestart(e_SetPiece restart) {
  return restart == e_SetPiece_ThrowIn || restart == e_SetPiece_GoalKick ||
         restart == e_SetPiece_Corner;
}

#endif
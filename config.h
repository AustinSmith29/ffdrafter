#ifndef CONFIG_H
#define CONFIG_H

// DRAFT CONFIGURATION CONSTANTS (edit this and recompile to change settings)
// =================================================================================================
#define PLAYER_CSV_LIST "projections_2021.csv" // file that has Player Names, Positions, and Projected Points

#define NUMBER_OF_TEAMS 12

#define NUMBER_OF_QB  1
#define NUMBER_OF_RB  0
#define NUMBER_OF_WR  0
#define NUMBER_OF_TE  1
#define NUMBER_OF_FLEX  0
#define NUMBER_OF_K  0
#define NUMBER_OF_DST  0

#define NUMBER_OF_PICKS  (NUMBER_OF_TEAMS * (NUMBER_OF_QB + \
		NUMBER_OF_RB + \
		NUMBER_OF_WR + \
		NUMBER_OF_TE + \
		NUMBER_OF_FLEX + \
		NUMBER_OF_K + \
		NUMBER_OF_DST))

// Sets order of first round of draft. Subsequent rounds are determined via snake draft.
// If you want the computer to make the optimal pick for this player, set the Controller as "AI".
// If the user wants to make the pick for that player, set the Controller as "HUMAN".
extern const char* DRAFT_ORDER[NUMBER_OF_TEAMS][2];
int team_with_pick(int pick);

void draftbot_initialize();
void draftbot_destroy();
#endif

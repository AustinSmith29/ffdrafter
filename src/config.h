#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

// DRAFT CONFIGURATION CONSTANTS (edit this and recompile to change settings)
// =================================================================================================
#define PLAYER_CSV_LIST "projections_2022.csv" // file that has Player Names, Positions, and Projected Points
#define MAX_PLAYERS 1000

#define NUMBER_OF_TEAMS 12

#define NUMBER_OF_QB  1
#define NUMBER_OF_RB  2
#define NUMBER_OF_WR  2
#define NUMBER_OF_TE  1
#define NUMBER_OF_FLEX  2
#define NUMBER_OF_K  0
#define NUMBER_OF_DST  0

#define NUMBER_OF_PICKS  (NUMBER_OF_TEAMS * (NUMBER_OF_QB + \
		NUMBER_OF_RB + \
		NUMBER_OF_WR + \
		NUMBER_OF_TE + \
		NUMBER_OF_FLEX + \
		NUMBER_OF_K + \
		NUMBER_OF_DST))

#define MAX_SLOT_NAME_LENGTH 10
#define MAX_NUM_SLOTS 15

/* A slot is a roster position that needs to be filled
 * in a lineup.
 *
 * If a Slot has flex defined than it can hold players
 * of the slot types listed in flex. 
*/
typedef struct Slot
{
    int num_required;
    char name[MAX_SLOT_NAME_LENGTH];
    int num_flex_options;
    int flex[MAX_NUM_SLOTS];
} Slot;

typedef struct DraftConfig
{
    int num_teams;
    int num_slots;
    Slot slots[MAX_NUM_SLOTS];
} DraftConfig;

const Slot* get_slot(const char* name, const DraftConfig* config);
bool is_flex_slot(const Slot* slot);

int load_config(DraftConfig* config, const char* filename);

// DraftConfig --> team_requirements

// Sets order of first round of draft. Subsequent rounds are determined via snake draft.
// If you want the computer to make the optimal pick for this player, set the Controller as "AI".
// If the user wants to make the pick for that player, set the Controller as "HUMAN".
extern const char* DRAFT_ORDER[NUMBER_OF_TEAMS][2];
int team_with_pick(int pick);

void draftbot_initialize();
void draftbot_destroy();
#endif

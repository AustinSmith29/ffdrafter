#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

// DRAFT CONFIGURATION CONSTANTS (edit this and recompile to change settings)
// =================================================================================================
#define PLAYER_CSV_LIST "projections_2022.csv" // file that has Player Names, Positions, and Projected Points
#define MAX_PLAYERS 1000

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
    int index; // This slot's index in the DraftConfig 'slots' array. Used to link Player pos to slot
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
bool flex_includes_position(const Slot* slot, int position);

int load_config(DraftConfig* config, const char* filename);
int get_number_of_picks(const DraftConfig* config);

int team_with_pick(int pick);

void draftbot_destroy();

#endif

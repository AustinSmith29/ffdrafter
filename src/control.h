#ifndef CONTROL_H
#define CONTROL_H

#include "players.h"
#include "config.h"

typedef struct DraftState 
{
    int pick;
    int still_required[NUMBER_OF_TEAMS][NUM_POSITIONS];
    Taken taken[NUMBER_OF_PICKS];
    int think_time;
} DraftState;

#define ERR_UNK_COMMAND -1
#define ERR_BAD_ARG -2
#define ERR_RUNTIME -3

#define QUIT 1

// Executes the command and modifies the draft state accordingly.
// Returns 0 on success. Returns error code on failure.
//
// ! Arguments are split on semicolons to make parsing easier because I'm a lazy sack of shit.
//
// Commands:
// -----------
// THINK_PICK
// MAKE_PICK;Player Name
// UNDO_PICK
// SET_THINK_TIME;time_in_seconds
// STATE -> Prints current_pick #, drafting team, and bot think_time.
// HISTORY
// ROSTER;team_id --> Shows roster slots and summation of all fantasy points for team
// POOL;position;lim --> Shows available players at position up to lim
// BENCH_PLAYER;Player Name --> Add player to taken list without assigning to roster (temporary hack
// to allow "bench" players... they don't add to score
// LOAD;filename --> loads and resumes draft that was previously started
// SAVE;filename --> saves draft
// SIM_DRAFT --> Computer picks every pick.
// EXIT
int do_command(char* command, DraftState* state);

void init_draftstate(DraftState* state);
#endif
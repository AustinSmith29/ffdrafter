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
// HISTORY;[?round]
// ROSTER;["Team Name"]
int do_command(char* command, DraftState* state);

#endif

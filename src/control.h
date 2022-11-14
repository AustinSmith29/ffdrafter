#ifndef CONTROL_H
#define CONTROL_H

#include "players.h"
#include "config.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0

typedef struct DraftState 
{
    int pick;
    Taken* taken;
    int think_time;
    int* still_required[];
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
// think_pick
// make_pick;Player Name
// undo_pick
// set_think_time;time_in_seconds
// state -> Prints current_pick #, drafting team, and bot think_time.
// history
// roster;team_id --> Shows roster slots and summation of all fantasy points for team
// pool;position;lim --> Shows available players at position up to lim
// bench_player;Player Name --> Add player to taken list without assigning to roster (temporary hack
// to allow "bench" players... they don't add to score
// load_draft_config;filename --> loads draft configuration from given file. Resets draft state.
// load;filename --> loads and resumes draft that was previously started
// save;filename --> saves draft
// sim_draft --> Computer picks every pick.
// exit --> Quit engine
int do_command(char* command, DraftState* state, const DraftConfig* config);

DraftState* init_draftstate(const DraftConfig* config);
void destroy_draftstate(DraftState* state, DraftConfig* config);
#endif

#ifndef CONTROL_H
#define CONTROL_H

#include "players.h"
#include "config.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 1
#define VERSION_PATCH 0

typedef struct DraftState 
{
    int pick;
    Taken* taken;
    int* still_required[];
} DraftState;

typedef struct Engine
{
    DraftState* state;
    DraftConfig* config;
    int think_time;
} Engine;

#define ERR_UNK_COMMAND -1
#define ERR_BAD_ARG -2
#define ERR_RUNTIME -3

#define QUIT 1

// Executes the command and modifies the draft state accordingly.
// Returns 0 on success. Returns error code on failure.
//
// Commands are documented in the README.
//
// ! Arguments are split on semicolons to make parsing easier because I'm a lazy sack of shit.
int do_command(char* command, Engine* engine);

void init_engine(Engine* engine);
void destroy_engine(Engine* engine);

DraftState* init_draftstate(const DraftConfig* config);
void destroy_draftstate(DraftState* state, const DraftConfig* config);
#endif

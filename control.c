#include "control.h"
#include "drafter.h"
#include "players.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void str_tolower(char* string);
static int dec_team_requirements(const PlayerRecord* const p, DraftState* state, int team);

int do_command(char* command, DraftState* state)
{
    //str_tolower(command); 
    char* token = strtok(command, ";");

    if (strcmp(token, "think_pick") == 0)
    {
        const PlayerRecord* p = calculate_best_pick(state->think_time, state->pick, state->taken);

        if (!p)
        {
            fprintf(stderr, "Error when trying to find best pick.\n");
            return ERR_RUNTIME;
        }

        fprintf(stdout, "%s\n", p->name);
        free(p);
        return 0;
    }
    else if (strcmp(token, "make_pick") == 0)
    {
        token = strtok(NULL, ";");
        if (!token)
        {
            fprintf(stderr, "make_pick requires a player name arg.\n");
            return ERR_BAD_ARG;
        }

        const PlayerRecord* p = get_player_by_name(token);
        if (!p)
        {
            fprintf(stderr, "No player with name %s exists.\n", token);
            return ERR_RUNTIME;
        }
        int team = team_with_pick(state->pick);
        if (dec_team_requirements(p, state, team) < 0)
        {
            fprintf(stderr, "No more slots in roster for player %s\n", p->name);
            return ERR_RUNTIME;
        }
        state->taken[state->pick++] = (Taken) { .player_id = p->id, .by_team = team };
        return 0;
    }
    else if (strcmp(token, "undo_pick") == 0)
    {
        return 0;
    }
    else if (strcmp(token, "set_think_time") == 0)
    {
        token = strtok(NULL, ";");
        if (!token)
        {
            fprintf(stderr, "set_think_time requires a time_in_seconds arg.\n");
            return ERR_BAD_ARG;
        }
        int new_time;
        if ((new_time = strtol(token, NULL, 10)) > 0)
        {
            state->think_time = new_time;
            return 0;
        }
        else
        {
            fprintf(stderr, "Think time must be an integer (in seconds)\n");
            return ERR_BAD_ARG;
        }
    }
    else if (strcmp(token, "state") == 0)
    {
        fprintf(stdout, "Pick: %d Drafting: %d Bot Think Time: %d\n", 
                state->pick, team_with_pick(state->pick), state->think_time);
        return 0;
    }
    else if (strcmp(token, "history") == 0)
    {
        return 0;
    }
    else if (strcmp(token, "roster") == 0)
    {
        return 0;
    }
    else
    {
        fprintf(stderr, "%s is an unknown command\n", token);
        return ERR_UNK_COMMAND;
    }
}

static void str_tolower(char* string)
{
    for (char* c = string; *c; c++) *c = tolower(*c);
}

static int dec_team_requirements(const PlayerRecord* const p, DraftState* state, int team)
{
    if ((p->position == RB || p->position == WR) && (state->still_required[team][FLEX] > 0)) {
        state->still_required[team][FLEX]--;
        return 0;
    }
    else if (state->still_required[team][p->position] > 0) {
        state->still_required[team][p->position]--;
        return 0;
    }
    return -1;
}

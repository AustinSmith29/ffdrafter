#include "control.h"
#include "drafter.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void str_tolower(char* string);
static int dec_team_requirements(const PlayerRecord* const p, DraftState* state, int team);
static void print_available(int position, int limit, const DraftState* const state);

// Parse the input and execute the corresponding logic.
int do_command(char* command, DraftState* state)
{
    char* token = strtok(command, ";");

    if (strcmp(token, "think_pick") == 0)
    {
        if (state->pick >= NUMBER_OF_PICKS)
        {
            fprintf(stderr, "No more slots available. Draft is complete.\n");
            return ERR_RUNTIME;
        }
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
        if (state->pick >= NUMBER_OF_PICKS)
        {
            fprintf(stderr, "No more slots available. Draft is complete.\n");
            return ERR_RUNTIME;
        }
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
        // Check if player is already taken
        for (int i = 0; i < state->pick; i++)
        {
            if (p->id == state->taken[i].player_id)
            {
                fprintf(stderr, "%s has already been picked.\n", p->name);
                return ERR_RUNTIME;
            }
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
        //TODO: Have to undo team_requirements too
        if (state->pick > 0) {
            state->pick--;
            const PlayerRecord* p = get_player_by_id(state->taken[state->pick].player_id);
            fprintf(stdout, "Undoing pick %d: %s\n", state->pick+1, p->name); //+1 so not zero index
        }
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
        for (int i = 0; i < state->pick; i++)
        {
            const PlayerRecord* p = get_player_by_id(state->taken[i].player_id);
            fprintf(stdout, "%d. %s\n", i+1, p->name);
        }
        return 0;
    }
    else if (strcmp(token, "roster") == 0)
    {
        token = strtok(NULL, ";");
        if (!token) 
        {
            fprintf(stderr, "roster requires a team_id arg.\n");
            return ERR_BAD_ARG;
        }
        int team_id = strtol(token, NULL, 10);;
        if (team_id < 0 || team_id > NUMBER_OF_TEAMS-1)
        {
            fprintf(stderr, "Team id must be an integer between 0 and %d\n", NUMBER_OF_TEAMS);
            return ERR_BAD_ARG;
        }

        //TODO: This is temporary. Eventually want to print out player names too and empty slots...
        //QB: Patrick Mahomes
        //RB: Dalvin Cook
        //RB: 
        //WR: Tyreek Hill
        //etc. etc.
        int* requirements = state->still_required[team_id];
        if (NUMBER_OF_QB > 0)
            printf("QB: %d\n", NUMBER_OF_QB - requirements[QB]);
        if (NUMBER_OF_RB > 0)
            printf("RB: %d\n", NUMBER_OF_RB - requirements[RB]);
        if (NUMBER_OF_WR > 0)
            printf("WR: %d\n", NUMBER_OF_WR - requirements[WR]);
        if (NUMBER_OF_TE > 0)
            printf("TE: %d\n", NUMBER_OF_TE - requirements[TE]);
        if (NUMBER_OF_FLEX > 0)
            printf("FLEX: %d\n", NUMBER_OF_FLEX - requirements[FLEX]);
        if (NUMBER_OF_K > 0)
            printf("K: %d\n", NUMBER_OF_K - requirements[K]);
        if (NUMBER_OF_DST > 0)
            printf("DST: %d\n", NUMBER_OF_DST - requirements[DST]);
        double sum = 0.0;
        for (int i = 0; i < state->pick; i++)
        {
            if (state->taken[i].by_team == team_id)
            {
                const PlayerRecord* player = get_player_by_id(state->taken[i].player_id);
                sum += player->projected_points;
            }
        }
        printf("Total Points: %f\n", sum);

        return 0;
    }
    else if (strcmp(token, "pool") == 0)
    {
        char* pos = strtok(NULL, ";"); // position
        if (!pos)
        {
            fprintf(stderr, "pool requires position arg\n");
            return ERR_BAD_ARG;
        }
        str_tolower(pos);

        char* limit_str = strtok(NULL, ";"); // limit
        if (!limit_str)
        {
            fprintf(stderr, "pool requires a limit arg after the position arg\n");
            return ERR_BAD_ARG;
        }
        int limit = strtol(limit_str, NULL, 10);;
        if (limit < 0)
        {
            fprintf(stderr, "Limit arg must be an integer.\n");
            return ERR_BAD_ARG;
        }

        if (strcmp(pos, "qb") == 0)
        {
            print_available(QB, limit, state);
        }
        else if (strcmp(pos, "rb") == 0)
        {
            print_available(RB, limit, state);
        }
        else if (strcmp(pos, "wr") == 0)
        {
            print_available(WR, limit, state);
        }
        else if (strcmp(pos, "te") == 0)
        {
            print_available(TE, limit, state);
        }
        else if (strcmp(pos, "k") == 0)
        {
            print_available(K, limit, state);
        }
        else if (strcmp(pos, "dst") == 0)
        {
            print_available(DST, limit, state);
        }
        else
        {
            fprintf(stderr, "Error: Unknown position passed\n");
            return ERR_BAD_ARG;
        }
        return 0;
    }
    else
    {
        fprintf(stderr, "%s is an unknown command\n", token);
        return ERR_UNK_COMMAND;
    }
}

void init_draftstate(DraftState* state)
{
    state->think_time = 10;
    state->pick = 0;
    memset(state->taken, 0, NUMBER_OF_PICKS * sizeof(Taken));
	for (int i = 0; i < NUMBER_OF_TEAMS; i++)
	{
		state->still_required[i][QB] = NUMBER_OF_QB;
		state->still_required[i][RB] = NUMBER_OF_RB;
		state->still_required[i][WR] = NUMBER_OF_WR;
		state->still_required[i][TE] = NUMBER_OF_TE;
		state->still_required[i][FLEX] = NUMBER_OF_FLEX;
		state->still_required[i][K] = NUMBER_OF_K;
		state->still_required[i][DST] = NUMBER_OF_DST;
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

static void print_available(int position, int limit, const DraftState* const state)
{
    PlayerRecord* p;
    for (p = players_begin(); p != players_end(); p = players_next())
    {
        if (limit == 0)
            break;
        if (p->position == position && !is_taken(p->id, state->taken, state->pick))
        {
            fprintf(stdout, "%s\n", p->name);
            limit--;
        }
    }
}

#include "control.h"
#include "drafter.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void str_tolower(char* string);
static int dec_team_requirements(const PlayerRecord* p, DraftState* state, int team, const DraftConfig* config);
static void print_available(int position, int limit, const DraftState* state);

// Parse the input and execute the corresponding logic.
int do_command(char* command, DraftState* state, const DraftConfig* config)
{
	//TODO: Clean this shit up
	int NUMBER_OF_PICKS = get_number_of_picks(config);
	int NUMBER_OF_TEAMS = config->num_teams;
	int NUMBER_OF_SLOTS = config->num_slots;
    char* token = strtok(command, ";");

    if (strcmp(token, "think_pick") == 0)
    {
        if (state->pick >= NUMBER_OF_PICKS)
        {
            fprintf(stderr, "No more slots available. Draft is complete.\n");
            return ERR_RUNTIME;
        }
        const PlayerRecord* p = calculate_best_pick(state->think_time, state->pick, state->taken, config);

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
        if (dec_team_requirements(p, state, team, config) < 0)
        {
            fprintf(stderr, "No more slots in roster for player %s\n", p->name);
            return ERR_RUNTIME;
        }
        state->taken[state->pick++] = (Taken) { .player_id = p->id, .by_team = team };
        return 0;
    }
    else if (strcmp(token, "undo_pick") == 0)
    {
        if (state->pick > 0) 
		{
            state->pick--;
            // Reset team requirements then redo picks to rebuild team_requirements.
            // We do this because I think its easier to this vs figure out if a WR/RB was
            // a FLEX or not.
            for (int i = 0; i < NUMBER_OF_TEAMS; i++)
            {
				for (int j = 0; j < NUMBER_OF_SLOTS; j++)
				{
					state->still_required[i][j] = config->slots[i].num_required;
				}
            }
            for (int i = 0; i < state->pick; i++)
            {
                const PlayerRecord* p = get_player_by_id(state->taken[i].player_id);
                int team = team_with_pick(i);
                dec_team_requirements(p, state, team, config);
            }
            const PlayerRecord* p = get_player_by_id(state->taken[state->pick].player_id);
            fprintf(stdout, "Undoing pick %d: %s\n", state->pick, p->name);
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
            fprintf(stdout, "%d. %s\n", i, p->name);
        }
        return 0;
    }
    else if (strcmp(token, "roster") == 0)
    {
		// TODO: Implement using new slot functionality SLOT, POS, NAME
		return 0;
    }
    else if (strcmp(token, "exit") == 0)
    {
        return QUIT;
    }
    else if (strcmp(token, "save") == 0)
    {
        char* filename = strtok(NULL, ";");
        if (!filename)
        {
            fprintf(stderr, "Error: No filename specified\n");
            return ERR_BAD_ARG;
        }
        FILE* f = fopen(filename, "w");
        if (!f)
        {
            fprintf(stderr, "%s\n", strerror(errno));
            return ERR_RUNTIME;
        }
        for (int i = 0; i < state->pick; i++)
        {
            char id_buf[10];
            if (snprintf(id_buf, 10, "%d\n", state->taken[i].player_id) < 0)
            {
                fclose(f);
                fprintf(stderr, "Error: Could not save file.\n");
                return ERR_RUNTIME;
            }
            fputs(id_buf, f);
        }
        fclose(f);
		return 0;
    }
    else if (strcmp(token, "load") == 0)
    {
        char* filename = strtok(NULL, ";");
        if (!filename)
        {
            fprintf(stderr, "Error: No filename specified\n");
            return ERR_BAD_ARG;
        }
        FILE* f = fopen(filename, "r");
        if (!f)
        {
            fprintf(stderr, "%s\n", strerror(errno));
            return ERR_BAD_ARG;
        }
        
		destroy_draftstate(state, config);
        state = init_draftstate(config); // reset draftstate
        int player_id;
        while (fscanf(f, "%d", &player_id) != EOF)
        {
            const PlayerRecord* p = get_player_by_id(player_id);
            int team = team_with_pick(state->pick);
            // Check if player is already taken
            for (int i = 0; i < state->pick; i++)
            {
                if (p->id == state->taken[i].player_id)
                {
                    fprintf(stderr, "%s has already been picked.\n", p->name);
                    return ERR_RUNTIME;
                }
            }
            if (dec_team_requirements(p, state, team, config) < 0)
            {
                fprintf(stderr, "No more slots in roster for player %s\n", p->name);
                return ERR_RUNTIME;
            }
            state->taken[state->pick++] = (Taken) { .player_id = p->id, .by_team = team };
        }
        return 0;
    }
    else if (strcmp(token, "sim_draft") == 0)
    {
        while (state->pick < NUMBER_OF_PICKS)
        {
            const PlayerRecord* p = calculate_best_pick(state->think_time, state->pick, state->taken, config);
            int team = team_with_pick(state->pick);
            dec_team_requirements(p, state, team, config);
            state->taken[state->pick++] = (Taken) { .player_id = p->id, .by_team = team };
            fprintf(stdout, "%s\n", p->name);
            free(p);
        }
		return 0;
    }
    else
    {
        fprintf(stderr, "%s is an unknown command\n", token);
        return ERR_UNK_COMMAND;
    }
}

DraftState* init_draftstate(const DraftConfig* config)
{
    assert(config != NULL);
	DraftState* state = malloc(sizeof(DraftState) + sizeof(int*) * config->num_teams);

	int n_picks = get_number_of_picks(config);
	state->taken = malloc(sizeof(Taken) * n_picks);
    memset(state->taken, 0, n_picks * sizeof(Taken));

    state->think_time = 10;
    state->pick = 0;

	for (int i = 0; i < config->num_teams; i++)
	{
		state->still_required[i] = malloc(sizeof(int) * config->num_slots);
		for (int j = 0; j < config->num_slots; j++)
		{
			state->still_required[i][j] = config->slots[j].num_required;
		}
	}

	return state;
}

void destroy_draftstate(DraftState* state, const DraftConfig* config)
{
	free(state->taken);
	for (int i = 0; i < config->num_teams; i++)
	{
		free(state->still_required[i]);
	}
	free(state);
}

static void str_tolower(char* string)
{
    for (char* c = string; *c; c++) *c = tolower(*c);
}

static int dec_team_requirements(const PlayerRecord* player, DraftState* state, int team, const DraftConfig* config)
{
	if (state->still_required[team][player->position] > 0)
	{
		state->still_required[team][player->position]--;
		return 0;
	}
	// Decrement first available flex position that fits this player's position
	else
	{
		for (int j = 0; j < config->num_slots; j++)
		{
			if (
				is_flex_slot(&config->slots[j]) && 
				state->still_required[team][j] > 0 &&
				flex_includes_position(&config->slots[j], player->position)
			   )
			{
				state->still_required[team][j]--;
			}
		}
		return 0;
	}

    return -1;
}

static void print_available(int position, int limit, const DraftState* state)
{
    const PlayerRecord* p = players_begin();
    for (; p != players_end(); p = players_next())
    {
        if (limit == 0)
            break;
        if (p->position == position && !is_taken(p->id, state->taken, state->pick))
        {
            fprintf(stdout, "%s %f\n", p->name, p->projected_points);
            limit--;
        }
    }
}

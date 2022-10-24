#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "players.h"

const char* DRAFT_ORDER[NUMBER_OF_TEAMS][2] = {
//  {Team Name, Controller}
    {"Nick Scardina", "AI"},
    {"Connor Hetterich", "AI"},
    {"Richard Kroupa", "AI"},
    {"Matt Everhart", "AI"},
    {"Austin Smith", "AI"},
    {"Christian Photos", "AI"},
    {"Daniel Rocco", "AI"},
    {"Alex Scardina", "AI"},
    {"Tyler Strong", "AI"},
    {"Liam Bramley", "AI"},
    {"Steven Sbash", "AI"},
    {"Cameron Urfer", "AI"}
};

static int SNAKE_DRAFT[NUMBER_OF_PICKS];

static void build_snake_order(void);

const Slot* get_slot(const char* name, const DraftConfig* config)
{
    Slot* slot;
    for (int i = 0; i < config->num_slots; i++)
    {
        if (strcmp(config->slots[i], name) == 0)
            return config->slots[i];
    }
    return NULL;
}

bool is_flex_slot(const Slot* slot)
{
    return slot->num_flex_options > 0;
}

void load_config(DraftConfig* config, const char* filename)
{
}

int team_with_pick(int pick)
{
	return SNAKE_DRAFT[pick];
}

void draftbot_initialize()
{
    if (load_players(PLAYER_CSV_LIST) < 0) {
        printf("Fatal error:  Could not load players!\n");
        exit(1);
    }
	build_snake_order();
}

void draftbot_destroy()
{
    unload_players();
}

static void build_snake_order(void)
{
	int snake = 0;
	int n_rounds = NUMBER_OF_PICKS / NUMBER_OF_TEAMS;
	for (int i = 0; i < n_rounds; i++) {
		int round_start_pick = i * NUMBER_OF_TEAMS;
		if (!snake)
		{
			for (int j = 0; j < NUMBER_OF_TEAMS; j++)
			{
				SNAKE_DRAFT[round_start_pick + j] = j;	
			}
		}
		else
		{
			int k = 0;
			for (int j = NUMBER_OF_TEAMS-1; j >= 0; j--, k++)
			{
				SNAKE_DRAFT[round_start_pick + k] = j;	
			}
		}
		snake = (!snake) ? 1 : 0;
	}

	// ANY DRAFT TRADES GO HERE!!!!
	// (is a hacky but easy solution to draft trades)
	// TODO: should we handle pick trades during draft?
	// i.e if Connor trades Nick for 1st overall pick before the draft:
	// SNAKE_DRAFT[0] = 1
	// SNAKE_DRAFT[1] = 0
}

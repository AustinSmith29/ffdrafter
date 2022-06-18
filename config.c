#include <stdio.h>

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
void build_snake_order()
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

int team_with_pick(int pick)
{
	return SNAKE_DRAFT[pick];
}

void draftbot_initialize()
{
    if (load_players("projections_2021.csv") < 0) {
        printf("Could not load players!\n");
    }
	build_snake_order();
}

void draftbot_destroy()
{
    unload_players();
}

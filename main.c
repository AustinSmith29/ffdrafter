//#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "players.h"
#include "drafter.h"
#include "config.h"

int main(int argc, char *argv[])
{
    /*
    // Read configuration (# of each position, # of AI drafters, draft order/names)
    // Read player csv list into memory
    // Do draft
    // Give post-draft results (Roster, total projected points, performance vs. optimal)
    initscr();
    const char* logo = 
       "   ___           _____  ___       __  \n"
       "  / _ \\_______ _/ _/ /_/ _ )___  / /_ \n"
       " / // / __/ _ `/ _/ __/ _  / _ \\/ __/ \n"
       "/____/_/  \\_,_/_/ \\__/____/\\___/\\__/  \n";

    attron(A_BLINK);
    printw(logo);
    printw("\nCreated by Austin Smith 2022\n");
    refresh();
    getch();
    endwin();
    */

	draftbot_initialize();

	Taken taken[NUMBER_OF_PICKS];

	int still_required[NUMBER_OF_TEAMS][NUM_POSITIONS];
	for (int i = 0; i < NUMBER_OF_TEAMS; i++)
	{
		still_required[i][QB] = NUMBER_OF_QB;
		still_required[i][RB] = NUMBER_OF_RB;
		still_required[i][WR] = NUMBER_OF_WR;
		still_required[i][TE] = NUMBER_OF_TE;
		still_required[i][FLEX] = NUMBER_OF_FLEX;
		still_required[i][K] = NUMBER_OF_K;
		still_required[i][DST] = NUMBER_OF_DST;
	}

	memset(taken, 0, NUMBER_OF_PICKS * sizeof(Taken));

	double team_points[NUMBER_OF_TEAMS];
	for (int i = 0; i < NUMBER_OF_TEAMS; i++) { team_points[i] = 0.0; }

	for (int i = 0; i < NUMBER_OF_PICKS; i++) {
		int team = team_with_pick(i);
		const PlayerRecord* player = NULL;
		if (strcmp(DRAFT_ORDER[team][1], "Human") == 0) 
		{
			while (player == NULL)
			{
				printf("Who did %s pick? ", DRAFT_ORDER[team][0]);
				char name[50];
				gets(name); // TODO: I know its unsafe... but this is just a test.
				player = get_player_by_name(name);
			}
		}
		else 
		{
			player = calculate_best_pick(5, i, taken);
		}
		taken[i].player_id = player->id;
		taken[i].by_team = team;
		team_points[team] += player->projected_points;
		printf("Round %d Team %s Picked %s\n", 
				NUMBER_OF_PICKS % NUMBER_OF_TEAMS, 
				DRAFT_ORDER[taken[i].by_team][0], 
				player->name
		);

		if ((player->position == RB || player->position == WR) && (still_required[team][FLEX] > 0)) {
			still_required[team][FLEX]--;
		}
		else {
			still_required[team][player->position]--;
		}
		if (still_required[team][player->position] < 0) {
			printf("Error\n");
			for (int j = 0; j < 7; j++) {
				printf("%d ", still_required[team][j]);
			}
			printf("\n");
		}
	}

	printf("\nDraft Completed!\n\nProjected Totals: \n");
	for (int i = 0; i < NUMBER_OF_TEAMS; i++)
	{
		printf("Team %s: %f \n", DRAFT_ORDER[taken[i].by_team][0], team_points[i]);
	}
    
	draftbot_destroy();
    return 0;
}

//#include <ncurses.h>
#include <stdio.h>
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

    if (load_players("projections_2021.csv") < 0) {
        printf("Could not load players!\n");
        return 1;
    }

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

	for (int i = 0; i < NUMBER_OF_PICKS; i++) {
		const PlayerRecord* player = calculate_best_pick(10, i, taken);
		taken[i].player_id = player->id;
		taken[i].by_team = i % NUMBER_OF_TEAMS;
		printf("Team %d Picked %s\n", taken[i].by_team, player->name);

		if ((player->position == RB || player->position == WR) && (still_required[i % NUMBER_OF_TEAMS][FLEX] > 0)) {
			still_required[i % NUMBER_OF_TEAMS][FLEX]--;
		}
		else {
			still_required[i % NUMBER_OF_TEAMS][player->position]--;
		}
		if (still_required[i % NUMBER_OF_TEAMS][player->position] < 0) {
			printf("Error\n");
			for (int j = 0; j < 7; j++) {
				printf("%d ", still_required[i % NUMBER_OF_TEAMS][j]);
			}
			printf("\n");
		}
	}
    
    unload_players();
    return 0;
}

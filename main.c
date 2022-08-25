//#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "control.h"
#include "players.h"
#include "drafter.h"
#include "config.h"

int main(int argc, char *argv[])
{

	draftbot_initialize();
    DraftState state;
    state.think_time = 10;
    state.pick = 0;

	Taken taken[NUMBER_OF_PICKS];

	int still_required[NUMBER_OF_TEAMS][NUM_POSITIONS];
	for (int i = 0; i < NUMBER_OF_TEAMS; i++)
	{
		state.still_required[i][QB] = NUMBER_OF_QB;
		state.still_required[i][RB] = NUMBER_OF_RB;
		state.still_required[i][WR] = NUMBER_OF_WR;
		state.still_required[i][TE] = NUMBER_OF_TE;
		state.still_required[i][FLEX] = NUMBER_OF_FLEX;
		state.still_required[i][K] = NUMBER_OF_K;
		state.still_required[i][DST] = NUMBER_OF_DST;
	}

	memset(state.taken, 0, NUMBER_OF_PICKS * sizeof(Taken));

	double team_points[NUMBER_OF_TEAMS];
	for (int i = 0; i < NUMBER_OF_TEAMS; i++) { team_points[i] = 0.0; }

    while (1) 
    {
        char cmd_buf[100];
        printf(">");
        fgets(cmd_buf, 100, stdin); 
        cmd_buf[strcspn(cmd_buf, "\n")] = '\0';
        int code = do_command(cmd_buf, &state);
    }

	printf("\nDraft Completed!\n\nProjected Totals: \n");
	for (int i = 0; i < NUMBER_OF_TEAMS; i++)
	{
		printf("Team %s: %f \n", DRAFT_ORDER[taken[i].by_team][0], team_points[i]);
	}
    
	draftbot_destroy();
    return 0;
}

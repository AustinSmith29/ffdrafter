//#include <ncurses.h>
#include <stdio.h>

#include "players.h"
#include "drafter.h"

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

    printf("Starting timer\n");
    const PlayerRecord* player = calculate_best_pick(5, NULL);
    printf("Done\n");
    
    unload_players();
    return 0;
}

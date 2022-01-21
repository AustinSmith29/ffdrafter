#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "players.h"

#define MAX_PLAYERS 1800 // little more than # of players on active NFL rosters (1696)

static PlayerRecord players[MAX_PLAYERS];
static int number_of_players = 0;

static int codify_position_str(const char* position_str);
int load_players(const char* csv_file)
{
    FILE* fp = fopen(csv_file, "r");
    const int MAXLINE = 150;

    if (!fp)
        return -1;

    char line[MAXLINE];
    while (fgets(line, MAXLINE, fp) != NULL)
    {
        const char* name = strtok(line, ",");
        const char* position_str = strtok(NULL, ",");
        const char* projected_points_str = strtok(NULL, ",");

        if (!name || !position_str || !projected_points_str)
            return -1;

        double projected_points = atof(projected_points_str);
        int position = codify_position_str(position_str);

        if (position < 0)
            return -1;

        char* heap_name = malloc(strlen(name + 1));
        if (!heap_name)
            return -1;
        strcpy(heap_name, name);

        PlayerRecord player = {
            .id = number_of_players,
            .projected_points = projected_points,
            .position = position,
            .name = heap_name
        };
        players[number_of_players++] = player;
    }

    fclose(fp);
}

void unload_players()
{
    PlayerRecord* player;
    for (player = players_begin(); player != players_end(); player = players_next())
    {
        free(player->name);
    }
}

int codify_position_str(const char* position_str)
{
    char* x;
    if ((x = strstr(position_str, "QB")) != NULL)
        return QB;
    else if ((x = strstr(position_str, "RB")) != NULL)
        return RB;
    else if ((x = strstr(position_str, "WR")) != NULL)
        return WR;
    else if ((x = strstr(position_str, "TE")) != NULL)
        return TE;
    else if ((x = strstr(position_str, "K")) != NULL)
        return K;
    else if ((x = strstr(position_str, "DST")) != NULL)
        return DST;
    else
        return -1;
}

const PlayerRecord* whos_highest_projected(char position, unsigned int taken[])
{
    return NULL;
}

static int iterator_counter = 0;
const PlayerRecord* players_begin()
{
    iterator_counter = 0;
    return &players[iterator_counter];
}

const PlayerRecord* players_end()
{
    return &players[number_of_players];
}

const PlayerRecord* players_next()
{
    return &players[++iterator_counter];
}

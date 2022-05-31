#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
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

        char* heap_name = malloc(strlen(name) + 1);
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
    return 0;
}

void unload_players()
{
    const PlayerRecord* player = players_begin();
    for (; player != players_end(); player = players_next())
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

int is_taken(int player_id, Taken taken[], int passed_picks)
{
	for (int i = 0; i < passed_picks; i++)
	{
		if (taken[i].player_id == player_id) {
			return 1;
		}
	}
	return 0;
}

const PlayerRecord* whos_highest_projected(int position, Taken taken[], int passed_picks)
{
	const PlayerRecord* player = players_begin();
	for (; player != players_end(); player = players_next()) 
	{
		if (player->position == position && !is_taken(player->id, taken, passed_picks))
		{
			return player;
		}
	}
    return NULL;
}

const PlayerRecord* get_player_by_id(unsigned int player_id)
{
	return &players[player_id];
}

const PlayerRecord* get_player_by_name(const char* name)
{
	const PlayerRecord* player = players_begin();
	for (; player != players_end(); player = players_next()) 
	{
		if (strcmp(player->name, name) == 0)
		{
			return player;
		}
	}
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

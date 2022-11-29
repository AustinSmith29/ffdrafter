#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
#include "players.h"

static PlayerRecord players[MAX_PLAYERS];
int number_of_players = 0;

static int codify_position_str(const char* position_str, const DraftConfig* config);
static bool does_player_match_slot(const PlayerRecord* player, const Slot* slot);

int load_players(const char* csv_file, const DraftConfig* config)
{
    FILE* fp = fopen(csv_file, "r");
    const int MAXLINE = 150;

    if (!fp)
        return -1;

    char line[MAXLINE];
    while (fgets(line, MAXLINE, fp) != NULL)
    {
        if (number_of_players > MAX_PLAYERS)
        {
            fprintf(stderr, "Cannot load any more players... Try changing MAX_PLAYERS in config.\n");
            return -1;
        }

        const char* name = strtok(line, ",");
        const char* position_str = strtok(NULL, ",");
        const char* projected_points_str = strtok(NULL, ",");

        if (!name || !position_str || !projected_points_str)
            return -1;

        double projected_points = atof(projected_points_str);
        int position = codify_position_str(position_str, config);

        if (position < 0) // Either position is not in slot list so we can ignore it or position is malformed
        {
            fprintf(stderr, "Warning: Player %s has position %s which could not be mapped to a slot. "
                    "Skipping player.\n", name, position_str);
            continue;
        }

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

int is_taken(int player_id, const Taken taken[], int passed_picks)
{
	for (int i = 0; i < passed_picks; i++)
	{
		if (taken[i].player_id == player_id) {
			return 1;
		}
	}
	return 0;
}

const PlayerRecord* whos_highest_projected(const Slot* slot, const Taken taken[], int passed_picks)
{
    double max_score = 0.0;
    const PlayerRecord* highest = NULL;
	const PlayerRecord* player = players_begin();
	for (; player != players_end(); player = players_next()) 
	{
        if (
            !is_taken(player->id, taken, passed_picks) && 
            player->projected_points > max_score &&
            does_player_match_slot(player, slot)
           )
        {
            highest = player;
            max_score = player->projected_points;
        }
	}
    return highest;
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

// Maps a player's position_str to the matching slot defined in the DraftConfig.
// Returns the index of the matching slot in the DraftConfig's slots array.
static int codify_position_str(const char* position_str, const DraftConfig* config)
{
    char* x;
    for (int i = 0; i < config->num_slots; i++)
    {
        if ((x = strstr(position_str, config->slots[i].name)) != NULL)
            return i;
    }
    return -1;
}

static bool does_player_match_slot(const PlayerRecord* player, const Slot* slot)
{
    if (is_flex_slot(slot))
    {
        for (int i = 0; i < slot->num_flex_options; i++)
        {
            if (player->position == slot->flex[i])
                return true;
        }
    }    
    return player->position == slot->index;
}

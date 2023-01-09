#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
#include "players.h"

static PlayerRecord players[MAX_PLAYERS];
int number_of_players = 0;
int slot_markers[MAX_NUM_SLOTS]; // points to index of first player in each slot section

static int player_compare(const void* a, const void* b);
static void group_by_position(PlayerRecord* players, const DraftConfig* config);
static int codify_position_str(const char* position_str, const DraftConfig* config);
static const Slot* slot_from_position_code(int position_code, const DraftConfig* config);
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
    // Here we sort the players from most to least projected points and then segment the table
    // according to the player's position.
    // This is done to dramatically increase the performance of whos_highest_projected, and thus,
    // the simulation phase of MCTS.
    qsort(players, number_of_players, sizeof(PlayerRecord), player_compare);
    group_by_position(players, config);
    printf("Loaded %d players.\n", number_of_players);

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

const PlayerRecord* whos_highest_projected(
        const Slot* slot, 
        const Taken taken[], 
        int passed_picks,
        const DraftConfig* config
        )
{
    // hacky way to cache taken so we don't have to call is_taken for every player which can get expensive
    // and not break existing interfaces.
    int taken_cache[1000] = {0};
    for (int i = 0; i < passed_picks; i++) 
        taken_cache[taken[i].player_id] = 1; // cache size needs to be > than biggest id

    // TODO: Temp solution to solving flex slots with this new method.
    if (is_flex_slot(slot))
    {
        double max_score = 0.0;
        const PlayerRecord* highest = NULL;
        for (int i = 0; i < slot->num_flex_options; i++)
        {
            const PlayerRecord* p = whos_highest_projected(
                    &config->slots[slot->flex[i]], 
                    taken, 
                    passed_picks,
                    config);
            if (p->projected_points > max_score)
            {
                highest = p;
                max_score = p->projected_points;
            }
        }
        return highest;
    }

    int slot_mark = slot_markers[slot->index];
    // TODO: Could calculate end of search bounds using next slot_mark instead of using number_of_players.
    for (int i = slot_mark; i < number_of_players; i++)
    {
        if (taken_cache[players[i].id] == 0)
            return &players[i];
    }
    return NULL;
    /*
    double max_score = 0.0;
    const PlayerRecord* highest = NULL;
	const PlayerRecord* player = players_begin();
	for (; player != players_end(); player = players_next()) 
	{
        if (
            taken_cache[player->id] < 0 && 
            player->projected_points > max_score &&
            does_player_match_slot(player, slot)
           )
        {
            highest = player;
            max_score = player->projected_points;
        }
	}
    return highest;
    */
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

int get_players_by_position(
        const char* position,
        PlayerRecord players[],
        const DraftConfig* config,
        int limit)
{
    int count = 0;
    const PlayerRecord* player = players_begin();
    for (; player != players_end(); player = players_next())
    {
        const Slot* slot = slot_from_position_code(player->position, config);
        if (!slot) continue;
        if (strcmp(slot->name, position) == 0 && count < limit)
        {
            players[count++] = *player;
        }
    }
    
    return count;
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

static int player_compare(const void* a, const void* b)
{
    const PlayerRecord* pa = (PlayerRecord*)a;
    const PlayerRecord* pb = (PlayerRecord*)b;
    if (pa->projected_points > pb->projected_points)
        return -1;
    else if (pa->projected_points < pb->projected_points)
        return 1;
    else
        return 0;
}

static void group_by_position(PlayerRecord* players, const DraftConfig* config)
{
    PlayerRecord* placeholder = malloc(sizeof(PlayerRecord) * number_of_players);
    int last_index = 0;
    // Scan over slots and transfer players over to the placeholder. Allows us to
    // "group" the players array by position.
    for (int i = 0; i < config->num_slots; i++)
    {
        // Flex positions are not real positions, therefore no player should have that
        // position on their PlayerRecord.
        if (is_flex_slot(&config->slots[i]))
            continue;

        int slot_index = i;
        bool set_marker = true;
        for (int j = 0; j < number_of_players; j++)
        {
            if (players[j].position == slot_index)
            {
                if (set_marker)
                {
                    slot_markers[slot_index] = last_index;
                    set_marker = false;
                }
                placeholder[last_index++] = players[j];
            }
        }
    }

    for (int i = 0; i < number_of_players; i++)
    {
        players[i] = placeholder[i];
        // reset id's because we want the id to correspond to array index for fast lookup
        players[i].id = i;
    }

    free(placeholder);
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

// Does the reverse mapping of codify_position_str. Takes a position string and returns
// the matching Slot.
// Returns NULL if slot with given code can't be found.
static const Slot* slot_from_position_code(int code, const DraftConfig* config)
{
    if (code < 0 || code >= config->num_slots)
        return NULL;
    return &config->slots[code];
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

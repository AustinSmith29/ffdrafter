#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>

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
static int find_slot_index(const char* position, const Slot* slots, int num_slots);

const Slot* get_slot(const char* name, const DraftConfig* config)
{
    Slot* slot;
    for (int i = 0; i < config->num_slots; i++)
    {
        if (strncmp(config->slots[i].name, name, MAX_SLOT_NAME_LENGTH) == 0)
            return &config->slots[i];
    }
    return NULL;
}

bool is_flex_slot(const Slot* slot)
{
    return slot->num_flex_options > 0;
}

int load_config(DraftConfig* draft, const char* filename)
{
    config_t config;
    config_setting_t* setting;

    config_init(&config);

    if (!config_read_file(&config, filename))
    {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&config),
                config_error_line(&config), config_error_text(&config));
        config_destroy(&config);
        return -1;
    }

    if (!config_lookup_int(&config, "number_of_teams", &draft->num_teams))
    {
        fprintf(stderr, "'number_of_teams' missing from config file or malformed.\n");
        return -1;
    }

    if ( (setting = config_lookup(&config, "slots") ) == NULL)
    {
        fprintf(stderr, "'slots' is missing from config file.\n");
        return -1;
    }

    int slot_count = config_setting_length(setting);
    if (slot_count > MAX_NUM_SLOTS)
    {
        fprintf(stderr, "Error: There are %d slots! The limit is %n!\n", slot_count, MAX_NUM_SLOTS);
        return -1;
    }
    draft->num_slots = slot_count;

    for (int i = 0; i < slot_count; i++)
    {
        config_setting_t* slot = config_setting_get_elem(setting, i);

        // 'name' and 'number_required' are required fields of a slot.
        // Don't return error code since we want to validate the rest of
        // the slots as well.
        const char* slot_name;
        int slot_num_required;
        if (!(config_setting_lookup_string(slot, "name", &slot_name) &&
             (config_setting_lookup_int(slot, "number_required", &slot_num_required))))
        {
            fprintf(stderr, "Config Error: Slot %i is missing 'name' or 'number_required'\n", i);
        }
        draft->slots[i].num_required = slot_num_required;
        strncpy(draft->slots[i].name, slot_name, MAX_SLOT_NAME_LENGTH);

        // For v1 we just make it so that flex slots must be defined at the end of the slot 
        // definitions as they must reference existing slots.
        config_setting_t* flex = config_setting_lookup(slot, "flex_slots");
        if (flex)
        {
            int num_flex_options = config_setting_length(flex);
            draft->slots[i].num_flex_options = num_flex_options;
            for (int j = 0; j < num_flex_options; j++)
            {
                const char* flex_position = config_setting_get_string_elem(flex, j);
                int index = find_slot_index(flex_position, draft->slots, slot_count);
                if (index < 0)
                {
                    fprintf(stderr, "Config Error: Flex Slot '%s': Could not find position '%s'\n", slot_name, flex_position);
                }
                else
                {
                    draft->slots[i].flex[j] = index;
                }
            }
        }
        else
        {
            draft->slots[i].num_flex_options = 0;
        }
    }

    config_destroy(&config);
    return 0;
}

int team_with_pick(int pick)
{
	return SNAKE_DRAFT[pick];
}

void draftbot_initialize()
{
    if (load_players(PLAYER_CSV_LIST) < 0) {
        fprintf(stderr, "Fatal error:  Could not load players!\n");
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

static int find_slot_index(const char* position, const Slot* slots, int num_slots)
{
    for (int i = 0; i < num_slots; i++)
    {
        if (strncmp(position, slots[i].name, MAX_SLOT_NAME_LENGTH) == 0)
        {
            return i;
        }
    }
    return -1;
} 

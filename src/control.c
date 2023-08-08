#include "control.h"
#include "drafter.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ARG_DELIM ";"

// Command functions
static int think_pick(const Engine* engine);
static int make_pick(Engine* engine);
static int undo_pick(Engine* engine);
static int set_think_time(Engine* engine);
static int state(const Engine* engine);
static int history(const Engine* engine);
static int roster(const Engine* engine);
static int available(const Engine* engine);
static int bench(Engine* engine);
static int give_pick(Engine* engine);
static int save(const Engine* engine);
static int load(Engine* engine);
static int load_draft_config(Engine* engine);
static int load_player_pool(Engine* engine);
static int sim_draft(Engine* engine);
static int do_exit();

// Helpers
static int fill_slot(const PlayerRecord* p, Engine* engine, int team);
static char* get_arg_str();
// Puts parsed integer in buf. Returns < 0 on error. 
static int get_arg_int(int* buf);
static int arg_error(const char* err_message);
static int runtime_error(const char* err_message);
static void get_players_at_pos_on_team(
        const Slot* slot, 
        int team, 
        const DraftState* state,
        PlayerRecord* players[],
        int num_required
);

// Parse the input and execute the corresponding logic.
int do_command(char* command_str, Engine* engine)
{
    if (strlen(command_str) < 1) {
        return 0;
    }
    char* command = strtok(command_str, ARG_DELIM);

    // Most commands require a draft configuration and a player pool to be loaded.
    // The only commands that do not are load_config and exit.
    // We will use this ready variable to limit the types of commands that can run.
    bool ready = engine->state != NULL && engine->config != NULL;

    if (strcmp(command, "load_config") == 0)
    {
        return load_draft_config(engine);
    }
    else if (strcmp(command, "exit") == 0)
    {
        return do_exit();
    }
    //----------------------------------------------
    // Commands that require the engine to be ready
    // ---------------------------------------------
    else if (strcmp(command, "load_players") == 0 && ready)
    {
        return load_player_pool(engine);
    }
    else if (strcmp(command, "think") == 0 && ready)
    {
        return think_pick(engine);
    }
    else if (strcmp(command, "pick") == 0 && ready)
    {
        return make_pick(engine);
    }
    else if (strcmp(command, "undo") == 0 && ready)
    {
        return undo_pick(engine);
    }
    else if (strcmp(command, "set_think_time") == 0 && ready)
    {
        return set_think_time(engine);
    }
    else if (strcmp(command, "state") == 0 && ready)
    {
        return state(engine);
    }
    else if (strcmp(command, "history") == 0 && ready)
    {
        return history(engine);
    }
    else if (strcmp(command, "roster") == 0 && ready)
    {
        return roster(engine);
    }
    else if (strcmp(command, "available") == 0 && ready)
    {
        return available(engine);
    }
    else if (strcmp(command, "bench") == 0 && ready)
    {
        return bench(engine);
    }
    else if (strcmp(command, "give_pick") == 0 && ready)
    {
        return give_pick(engine);
    }
    else if (strcmp(command, "save") == 0 && ready)
    {
        return save(engine);
    }
    else if (strcmp(command, "load") == 0 && ready)
    {
        return load(engine);
    }
    else if (strcmp(command, "sim") == 0 && ready)
    {
        return sim_draft(engine);
    }
    else if (!ready)
    {
        fprintf(stdout, "Engine is not ready. Load a configuration file AND a player pool.\n");
        return ERR_RUNTIME;
    }
    else
    {
        fprintf(stderr, "%s is an unknown command\n", command);
        return ERR_UNK_COMMAND;
    }
}

void init_engine(Engine* engine)
{
    engine->state = NULL;
    engine->config = NULL;
    engine->think_time = 10;
}

void destroy_engine(Engine* engine)
{
    if (engine->state)
        destroy_draftstate(engine->state, engine->config);
    if (engine->config)
        free(engine->config);
    destroy_players();
}

DraftState* init_draftstate(const DraftConfig* config)
{
    assert(config != NULL);
	DraftState* state = malloc(sizeof(DraftState) + sizeof(int*) * config->num_teams);

	int n_picks = get_number_of_picks(config);
	state->taken = malloc(sizeof(Taken) * n_picks);
    memset(state->taken, 0, n_picks * sizeof(Taken));

    state->pick = 0;

	for (int i = 0; i < config->num_teams; i++)
	{
		state->still_required[i] = malloc(sizeof(int) * config->num_slots);
		for (int j = 0; j < config->num_slots; j++)
		{
			state->still_required[i][j] = config->slots[j].num_required;
		}
	}

	return state;
}

void destroy_draftstate(DraftState* state, const DraftConfig* config) 
{
	free(state->taken);
	for (int i = 0; i < config->num_teams; i++)
	{
		free(state->still_required[i]);
	}
	free(state);
}

static int fill_slot(const PlayerRecord* player, Engine* engine, int team)
{
	if (engine->state->still_required[team][player->position] > 0)
	{
		engine->state->still_required[team][player->position]--;
		return 0;
	}
	// Decrement first available flex position that fits this player's position
	else
	{
		for (int j = 0; j < engine->config->num_slots; j++)
		{
			if (
				is_flex_slot(&engine->config->slots[j]) && 
				engine->state->still_required[team][j] > 0 &&
				flex_includes_position(&engine->config->slots[j], player->position)
			   )
			{
				engine->state->still_required[team][j]--;
                break; // don't wanna fill up multiple flex's if possible
			}
		}
		return 0;
	}

    return -1;
}

static int think_pick(const Engine* engine)
{
    if (engine->state->pick > get_number_of_picks(engine->config))
        return runtime_error("No more slots available. Draft is complete.");

    const PlayerRecord* player = calculate_best_pick(
            engine->think_time,
            engine->state->pick,
            engine->state->taken,
            engine->config
    );

    if (!player)
        return runtime_error("Could not calculate best pick.");

    fprintf(stdout, "%s\n", player->name);

    return 0;
}

static int make_pick(Engine* engine)
{
    if (engine->state->pick > get_number_of_picks(engine->config))
        return runtime_error("No more slots available. Draft is complete.");

    const char* player_name = get_arg_str();
    if (!player_name)
        return arg_error("make_pick requires a player name argument.");

    const PlayerRecord* player = get_player_by_name(player_name);
    if (!player)
        return runtime_error("No player exists with that name.");

    if (is_taken(player->id, engine->state->taken, engine->state->pick))
        return runtime_error("That player has already been picked.");

    if (fill_slot(player, engine, team_with_pick(engine->state->pick)) < 0)
        return runtime_error("No roster slots available for that player.");

    engine->state->taken[engine->state->pick] = (Taken) {
        .player_id = player->id,
        .by_team = team_with_pick(engine->state->pick)
    };
    engine->state->pick++;

    return 0;
}

static int undo_pick(Engine* engine)
{
    // Nothing to undo so get outta here
    if (engine->state->pick <= 0)
        return 0;

    // Reset team requirements then redo picks to rebuild team_requirements.
    // We do this because I think its easier to do this vs figure out if a player was
    // used to fill a FLEX or not.
    for (int i = 0; i < engine->config->num_teams; i++)
    {
        // Reset team_requirements
        for (int j = 0; j < engine->config->num_slots; j++)
        {
            engine->state->still_required[i][j] = engine->config->slots[i].num_required;
        }
    }

    // Play back draft up to the pick before last, effectively undoing the last pick.
    engine->state->pick--;
    for (int i = 0; i < engine->state->pick; i++)
    {
        const PlayerRecord* player = get_player_by_id(engine->state->taken[i].player_id);
        fill_slot(player, engine, team_with_pick(i));
    }
    
    return 0;
}

static int set_think_time(Engine* engine)
{
    int think_time;
    if (get_arg_int(&think_time) < 0)
        return arg_error("set_think_time requires a time_in_seconds argument.");

    if (think_time <= 0)
        return arg_error("The think time must be an integer (in seconds).");

    engine->think_time = think_time;

    return 0;
}

static int state(const Engine* engine)
{
    fprintf(stdout, "Pick: %d | Drafting: %d | Engine Think Time: %d\n",
            engine->state->pick, team_with_pick(engine->state->pick), engine->think_time);

    return 0;
}

static int history(const Engine* engine)
{
    for (int i = 0; i < engine->state->pick; i++)
    {
        const PlayerRecord* player = get_player_by_id(engine->state->taken[i].player_id);
        fprintf(stdout, "%d. %s\n", i, player->name);
    }

    return 0;
}

static int roster(const Engine* engine)
{
    int team;
    if (get_arg_int(&team) < 0)
        return arg_error("roster requires a team_id argument.");

    fprintf(stdout, "Team %d\n============\n", team);
    for (int i = 0; i < engine->config->num_slots; i++)
    {
        int num_required = engine->config->slots[i].num_required;
        PlayerRecord* players[num_required];
        get_players_at_pos_on_team(&engine->config->slots[i], team, engine->state, players, num_required);
        for (int j = 0; j < num_required; j++)
        {   
            if (players[j])
                fprintf(stdout, "%s: %s\n", engine->config->slots[i].name, players[j]->name);
            else
                fprintf(stdout, "%s:\n", engine->config->slots[i].name);
        }
        for (int i = 0; i < num_required; i++)
        {
            if (players[i])
                free(players[i]);
        }
    }
    double score = 0.0;
    for (int i = 0; i < engine->state->pick; i++)
    {
        if (engine->state->taken[i].by_team == team)
        {
            const PlayerRecord* player = get_player_by_id(engine->state->taken[i].player_id);
            score += player->projected_points;
        }
    }
    fprintf(stdout, "Projected points: %f\n", score);

    return 0;
}

static int available(const Engine* engine)
{
    const char* position = get_arg_str();
    if (!position)
        return runtime_error("available command requires a position argument");

    // Parse and set optional limit argument which limits the number of available players to show.
    int limit = 10; // default
    int limit_arg;
    if (get_arg_int(&limit_arg) == 0)
        limit = (limit_arg > 0) ? limit_arg : limit;

    PlayerRecord players[limit];
    int num_players = get_players_by_position(position, players, engine->config, limit);

    for (int i = 0; i < num_players; i++)
    {
        fprintf(stdout, "%s\n", players[i].name);
    }

    return 0;
}

// We needed a way to account for the fact that many drafts have "bench" slots. These
// bench slots do not count toward the starting lineups total fantasy points. To simulate
// this, we just mark the player as taken and advance the pick.
static int bench(Engine* engine)
{
    if (engine->state->pick >= get_number_of_picks(engine->config))
        return runtime_error("No more slots are available. Draft is complete.");

    const char* player_name = get_arg_str();
    if (!player_name)
        return arg_error("bench requires a player name argument.");

    const PlayerRecord* player = get_player_by_name(player_name);
    if (!player)
        return runtime_error("No player exists with that name.");

    if (is_taken(player->id, engine->state->taken, engine->state->pick))
        return runtime_error("That player has already been picked.");

    engine->state->taken[engine->state->pick] = (Taken) {
        .player_id = player->id,
        .by_team = team_with_pick(engine->state->pick)
    };

    engine->state->pick++;

    return 0;
}

static int give_pick(Engine* engine)
{
	int n_picks = get_number_of_picks(engine->config);

    int pick_num;
    if (get_arg_int(&pick_num) < 0)
        return arg_error("give_pick requires a pick number argument.");
    if (pick_num > n_picks-1 || pick_num < engine->state->pick)
        return arg_error("Cannot trade that pick.");

    int team_id;
    if (get_arg_int(&team_id) < 0)
        return arg_error("give_pick requires a team_id argument.");
    if (team_id < 0 || team_id > engine->config->num_teams-1)
        return arg_error("invalid team_id");

    assign_pick(pick_num, team_id);

    return 0;
}

static int save(const Engine* engine)
{
    char* filename = get_arg_str();
    if (!filename)
        return arg_error("save requires a filename argument.");
    
    FILE* f = fopen(filename, "w");
    if (!f)
        return runtime_error(strerror(errno));

    // Draft save files are just a list of taken player ids in the order
    // that they were picked. TODO: Should we reference the config file/player_pool file
    // that this draft used?
    for (int i = 0; i < engine->state->pick; i++)
    {
        char id_buf[10];
        if (snprintf(id_buf, 10, "%d\n", engine->state->taken[i].player_id) < 0)
        {
            fclose(f);
            return runtime_error("Could not save file.");
        }

        fputs(id_buf, f);
    }

    fclose(f);

    return 0;
}

static int load(Engine* engine)
{
    char* filename = get_arg_str();
    if (!filename)
        return arg_error("load requires a filename argument.");

    FILE* f = fopen(filename, "r");
    if (!f)
        return runtime_error(strerror(errno));

    // Reset draft state 
    destroy_draftstate(engine->state, engine->config);
    engine->state = init_draftstate(engine->config);

    // Every line in the file is a player id in the order in
    // which the players were picked. So first line is the first
    // pick, second line is second pick... and so on. So we
    // just "play" back the draft to load it.
    int player_id;
    while (fscanf(f, "%d", &player_id) != EOF)
    {
        const PlayerRecord* player = get_player_by_id(player_id);

        if (!player)
        {
            fclose(f);
            return runtime_error("Loading Error. Player not found.");
        }

        if (is_taken(player->id, engine->state->taken, engine->state->pick))
        {
            fclose(f);
            return runtime_error("Loading Error. Player has already been picked.");
        }

        if (fill_slot(player, engine, team_with_pick(engine->state->pick)) < 0)
        {
            fclose(f);
            return runtime_error("Loading Error. No more slots in roster for that player.");
        }
        engine->state->taken[engine->state->pick] = (Taken) {
            .player_id = player->id,
            .by_team = team_with_pick(engine->state->pick)
        };

        engine->state->pick++;
    }

    fclose(f);

    return 0;
}

static int load_draft_config(Engine* engine)
{
    const char* filename = get_arg_str();
    if (!filename)
        return arg_error("load_draft_config requires filename argument.");

    const DraftConfig* new_config = load_config(filename);
    if (!new_config)
    {
        free(new_config);
        return runtime_error("Failed to load configuration file.");
    }

    // Loading a new configuration requires us to reset the draftstate
    if (engine->state)
        destroy_draftstate(engine->state, engine->config);

    engine->state = init_draftstate(new_config);
    engine->config = new_config;

    return 0;
}

static int load_player_pool(Engine* engine)
{
    const char* filename = get_arg_str();
    if (!filename)
        return arg_error("load_player_pool requires filename argument.");

    if (load_players(filename, engine->config) < 0)
        return runtime_error("Could not load players.");

    destroy_draftstate(engine->state, engine->config);
    engine->state = init_draftstate(engine->config);

    return 0;
}

static int sim_draft(Engine* engine)
{
    int n_picks = get_number_of_picks(engine->config);
    while (engine->state->pick < n_picks)
    {
        const PlayerRecord* player = calculate_best_pick(
                engine->think_time,
                engine->state->pick,
                engine->state->taken,
                engine->config
        );

        fill_slot(player, engine, team_with_pick(engine->state->pick));
        engine->state->taken[engine->state->pick] = (Taken) {
            .player_id = player->id,
            .by_team = team_with_pick(engine->state->pick)
        };
        fprintf(stdout, "%s\n", player->name);
        engine->state->pick++;
    }

    return 0;
}

static int do_exit()
{
    return QUIT;
}

static char* get_arg_str()
{
    return strtok(NULL, ARG_DELIM);
}

static int get_arg_int(int* buf)
{
    const char* arg = get_arg_str();
    if (!arg)
        return -1;

    *buf = strtol(arg, NULL, 10);
    return 0;
}

static int arg_error(const char* err_message)
{
    fprintf(stderr, "Bad Argument Error: %s\n", err_message);
    return ERR_BAD_ARG;
}

static int runtime_error(const char* err_message)
{
    fprintf(stderr, "Error: %s\n", err_message);
    return ERR_RUNTIME;
}

static void get_players_at_pos_on_team(
        const Slot* slot, 
        int team, 
        const DraftState* state,
        PlayerRecord* players[],
        int num_required
)
{
    // initially set all players to NULL
    for (int i = 0; i < num_required; i++) players[i] = NULL;

    int count = 0;
    for (int i = 0; i < state->pick; i++)
    {
        if (state->taken[i].by_team == team && count < num_required)
        {
            const PlayerRecord* player = get_player_by_id(state->taken[i].player_id);
            if (player->position == slot->index || 
                (is_flex_slot(slot) && flex_includes_position(slot, player->position)))
            {
                PlayerRecord* copy = malloc(sizeof(PlayerRecord));
                memcpy(copy, player, sizeof(PlayerRecord));
                players[count++] = copy;
            }
        }
    }
}

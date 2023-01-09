#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "drafter.h"
#include "config.h"

// Globals which are set by the corresponding values the passed DraftConfig to 
// calculate_best_pick. These values get set at the beginning of 
// calculate_best_pick and SHOULD NOT BE MODIFIED.
static int NUMBER_OF_TEAMS = 0;
static int NUMBER_OF_SLOTS = 0;
static int NUMBER_OF_PICKS = 0;

// Zscores get calculated and stashed in this array at beginning 
// of calculate_best_pick.
// TODO: Ideally we would dynamically allocate this to the number of
// players in the player pool but just using a static buffer is alright
// for now.
static double zscores[1000];

typedef struct Node
{
    int visited;
    double* scores;
    const PlayerRecord* chosen_player;
    struct Node* parent;
	struct Node* children[];
} Node;

// We will be going down "experimental" branches of draft trees
// and will need a way to keep track/reset search state when
// we want to switch to a different branch. This structure
// packs all this context data together.
typedef struct SearchContext
{
	Node* node;
	int pick;
	Taken* taken;
    int* team_requirements[];
} SearchContext;

static Node* create_node(Node* parent, const PlayerRecord* chosen_player);
static void free_node(Node *node);

static SearchContext* create_search_context(int pick, const Taken* taken, const DraftConfig* config);
static void destroy_search_context(SearchContext* context);
static void reset_search_context_to(const SearchContext* original, SearchContext* delta);

static Node* select_child(const Node* parent, int team);
static double calculate_ucb(const Node* node, int team);
static bool is_leaf(const Node* node);

// Decrement number of still required from a team's roster requirements 
// based on the player's position and the remaining slots available on 
// team's roster. Will automatically fill a FLEX slot if necessary. Does NOT
// affect the context's "taken" state or increment the current context's pick.
static void fill_slot(SearchContext* context, const PlayerRecord* player, int team, const DraftConfig* config);

// Marks player as taken in the given context, calls fill_slot to update the
// team's roster requirements, and increments the context's pick state.
static void make_pick(SearchContext* context, const PlayerRecord* player, const DraftConfig* config);

// Creates next level of tree from the passed leaf node. Creates 
// NUMBER_OF_SLOTS new children where each child represents picking the player
// at that slot with the highest point total.
static void expand_tree(Node* node, const SearchContext* context, const DraftConfig* config);

// What we are measuring is not raw score but rather score share. For example, if we were simply maximizing
// the drafting player's score, we could have the following opportunity:
//
//    Team A (drafting team)  |   Team B       |  Team C
//    -----------------------------------------------------
//          1000 points       |   900 points   | 800 points
//
// Compare that with:
//    
//    Team A (drafting team)  |   Team B       |  Team C
//    -----------------------------------------------------
//          1000 points        |   850 points   |  850 points
//
// Team A still has the same percentage of the score pie, but has limited the strength of the next best
// team, which in my opinion is preferable. Granted, it accomplished this by increasing the strength of
// Team C, but it brings the opponents down to an equilibrium minima (here I'm using big words to 
// disguise the fact that I'm not 100% sure about the validity of my statement and am instead relying
// on intuition.) AKA... Just trust me bro.
//
// So the returned value of this function is actually a value in the range [0, 1] which corresponds with
// the percentage of the total score "pie" obtained for the player.
static double simulate_score(const SearchContext* context, const Node* from_node, const DraftConfig* config);

// These functions are responsible for the simulation phase of the monte carlo search. These have very
// important ramifications on the performance of the algorithm. My methodology when simming a single pick
// is to randomly pick between 3 drafting methods:
//    1. highest zscore: heuristic to pick the player with best "value"
//    2. highest score: greedily pick player with highest points
//    3. random: pick a random position and then pick the highest projected player in that position
//
// Pure MCTS calls for just the random pick method, but experimentally I have discovered that adding
// the zscore method significantly improved the quality of the picks.
static const PlayerRecord* sim_pick_for_team(const SearchContext* context, const DraftConfig* config);
static const PlayerRecord* zscore_pick_method(const SearchContext* context, const DraftConfig* config);
static const PlayerRecord* greedy_pick_method(const SearchContext* context, const DraftConfig* config);
static const PlayerRecord* random_pick_method(const SearchContext* context, const DraftConfig* config);

static void backpropogate_score(Node* node, double score, int team);
static void calculate_zscores(const DraftConfig* config);


// Uses the Monte Carlo Tree Search Algorithm to find which available player 
// maximizes the teams total projected fantasy points.
// More on MCTS: https://www.geeksforgeeks.org/ml-monte-carlo-tree-search-mcts/
const PlayerRecord* calculate_best_pick(
    int thinking_time, 
    int pick, 
    Taken taken[], 
    const DraftConfig* draft_config)
{
    // Set globals from values in draft_config
    NUMBER_OF_SLOTS = draft_config->num_slots;
    NUMBER_OF_TEAMS = draft_config->num_teams;
    NUMBER_OF_PICKS = get_number_of_picks(draft_config);
    calculate_zscores(draft_config);

	srand(time(NULL));
    clock_t start_time_s = clock() / CLOCKS_PER_SEC;
    if (start_time_s < 0)
    {
        return NULL;
    }

	// MASTER_CONTEXT reflects the real state of the draft i.e Actual current pick in the draft and
	// actual taken players outside of this function.
	SearchContext* MASTER_CONTEXT = create_search_context(pick, taken, draft_config);
	SearchContext* current_context = create_search_context(pick, taken, draft_config);

    Node* root = create_node(NULL, NULL);

	MASTER_CONTEXT->node = root;
	current_context->node = root;
    int max_depth = 0;
    while ( (clock() / CLOCKS_PER_SEC) - start_time_s < thinking_time )
    {
        Node* node = current_context->node;

        // If node is NULL then that indicates we've searched the entire
        // search space, therefore we are done searching.
        if (!node)
            break;

        node->visited++;

        if (is_leaf(node))
        {
            expand_tree(node, current_context, draft_config);
            if (node->parent != NULL) // We don't calculate score for root
            {
                double score = simulate_score(current_context, node, draft_config);
                backpropogate_score(node, score, team_with_pick(current_context->pick)); 

                if (current_context->pick > max_depth)
                    max_depth = current_context->pick;
            }
			reset_search_context_to(MASTER_CONTEXT, current_context);
        }
        else
        {
            if (node != root) // root doesn't have a player associated to it
                make_pick(current_context, node->chosen_player, draft_config);

            current_context->node = select_child(node, team_with_pick(current_context->pick));
        }
    } 

	// Destroying context removes node so we have to copy which player
	// it would have taken.
	double max = 0.0;
	int child = 0;
	int team = team_with_pick(pick);
	for (int i = 0; i < NUMBER_OF_SLOTS; i++)
	{
		if (root->children[i] && root->children[i]->scores[team] > max)
		{
			max = root->children[i]->scores[team];
			child = i;
		}
	}
    if (root->children[child] == NULL) 
    {
        destroy_search_context(MASTER_CONTEXT);
        destroy_search_context(current_context);
        free_node(root);
        return NULL;
    }

    const PlayerRecord* chosen_player = get_player_by_id(root->children[child]->chosen_player->id);

    printf("Max depth: %d\n", max_depth);
    printf("Iterations: %d\n", root->visited);

	destroy_search_context(MASTER_CONTEXT);
	destroy_search_context(current_context);

	free_node(root);

    return chosen_player;
}

static Node* create_node(Node* parent, const PlayerRecord* chosen_player)
{
    Node* node = malloc(sizeof(Node) + NUMBER_OF_SLOTS * sizeof(Node*));

    node->parent = parent;
    node->visited = 0;
    node->chosen_player = chosen_player;
    node->scores = malloc(sizeof(double) * NUMBER_OF_TEAMS);

	for (int i = 0; i < NUMBER_OF_TEAMS; i++) 
    	node->scores[i] = 0.0;
    for (int i = 0; i < NUMBER_OF_SLOTS; i++)
        node->children[i] = NULL;

    return node;
}

static void free_node(Node* node)
{
	if (!node)
		return;

    free(node->scores);

    for (int i = 0; i < NUMBER_OF_SLOTS; i++)
        free_node(node->children[i]);

    free(node);
}

static SearchContext* create_search_context(int pick, const Taken* taken, const DraftConfig* config)
{
	SearchContext* context = malloc(sizeof(SearchContext) + sizeof(int*) * NUMBER_OF_TEAMS);
	context->node = NULL;
	context->pick = pick;
    context->taken = malloc(sizeof(Taken) * NUMBER_OF_PICKS);
	memcpy(context->taken, taken, NUMBER_OF_PICKS * sizeof(Taken));
	for (int i = 0; i < NUMBER_OF_TEAMS; i++)
	{
        context->team_requirements[i] = malloc(sizeof(int) * NUMBER_OF_SLOTS);
		for (int j = 0; j < NUMBER_OF_SLOTS; j++)
		{
			context->team_requirements[i][j] = config->slots[j].num_required;
		}
	}
	// Loop through taken and mark off those players from team_requirements
	for (int i = 0; i < pick; i++) 
	{
		Taken t = taken[i];
		const PlayerRecord* player = get_player_by_id(t.player_id);
		fill_slot(context, player, t.by_team, config);
	}
	
	return context;
}

static void destroy_search_context(SearchContext* context)
{
	free(context->taken);
	for (int i = 0; i < NUMBER_OF_TEAMS; i++)
		free(context->team_requirements[i]);
	free(context);
}

static void reset_search_context_to(const SearchContext* original, SearchContext* delta)
{
	delta->node = original->node;
	delta->pick = original->pick;
	memcpy(delta->taken, original->taken, NUMBER_OF_PICKS * sizeof(Taken));
	for (int i = 0; i < NUMBER_OF_TEAMS; i++)
		memcpy(delta->team_requirements[i], original->team_requirements[i], NUMBER_OF_SLOTS * sizeof(int));
}

static Node* select_child(const Node* parent, int team)
{
	assert(parent != NULL);
	Node* max_score_node = NULL;
	double max_score = 0.0;
	for (int i = 0; i < NUMBER_OF_SLOTS; i++) 
	{
		Node* child = parent->children[i];
		if (!child) 
			continue;

		if (child->visited == 0) 
		{
			max_score_node = child;
			break;
		}
		double score = calculate_ucb(child, team);
		if (score > max_score)
		{
			max_score = score;
			max_score_node = child;
		}
	}
	return max_score_node;
}

static double calculate_ucb(const Node* node, int team)
{
	assert(node != NULL);
	double total = 0.0;
	for (int i = 0; i < NUMBER_OF_SLOTS; i++) 
	{
		const Node* child = node->children[i];
		if (child)
			total += child->scores[team];
	}
	double mean_score = total / NUMBER_OF_SLOTS;
	return mean_score + 2 * sqrt(log(node->parent->visited) / node->visited);
}

static bool is_leaf(const Node* node)
{
	assert(node != NULL);
    for (int i = 0; i < NUMBER_OF_SLOTS; i++)
    {
        if (node->children[i])
        {
            return false;
        }
    }
    return true;
}

static void fill_slot(SearchContext* context, const PlayerRecord* player, int team, const DraftConfig* config)
{
	if (context->team_requirements[team][player->position] > 0)
	{
		context->team_requirements[team][player->position]--;
	}
	// Decrement first available flex position that fits this player's position
	else
	{
		for (int j = 0; j < NUMBER_OF_SLOTS; j++)
		{
			if (
				is_flex_slot(&config->slots[j]) && 
				context->team_requirements[team][j] > 0 &&
				flex_includes_position(&config->slots[j], player->position)
			   )
			{
				context->team_requirements[team][j]--;
			}
		}
	}
}

static void make_pick(SearchContext* context, const PlayerRecord* player, const DraftConfig* config)
{
    assert(player != NULL);
	int team = team_with_pick(context->pick);
	context->taken[context->pick].player_id = player->id;
	context->taken[context->pick].by_team = team;
	fill_slot(context, player, team, config);
    context->pick++;
}

static void expand_tree(Node* const node, const SearchContext* context, const DraftConfig* config)
{
	assert(node != NULL);

    // next tree level is for *next* pick.
	// Need to add the node we are coming from to the pick history. Ran into a bug where the team
    // that drafted back-to-back in the snake draft was thinking that the player in the from_node was
    // still available to be picked. Here we copy the search_context and then expand.
    SearchContext* expand_context = create_search_context(context->pick, context->taken, config);
    reset_search_context_to(context, expand_context);
    if (node->chosen_player)
        make_pick(expand_context, node->chosen_player, config);

    if (expand_context->pick >= NUMBER_OF_PICKS) 
    {
        destroy_search_context(expand_context);
        return;
    }

	const int* requirements = expand_context->team_requirements[team_with_pick(expand_context->pick)];
	for (int i = 0; i < NUMBER_OF_SLOTS; i++)
	{
		const PlayerRecord* player;
        const Slot* slot = &config->slots[i];
		if (requirements[i] > 0 && (player = whos_highest_projected(slot, expand_context->taken, expand_context->pick, config)) != NULL)
			node->children[i] = create_node(node, player);
	}

    destroy_search_context(expand_context);
}

static double simulate_score(const SearchContext* context, const Node* from_node, const DraftConfig* config)
{
	// Copy search context so we can simulate in isolation
	SearchContext* sim_search_context = create_search_context(context->pick, context->taken, config);
	reset_search_context_to(context, sim_search_context);

	unsigned int drafting_team = team_with_pick(context->pick);

	// Assume pick from from_node happened and sim remaining rounds
	make_pick(sim_search_context, from_node->chosen_player, config);

	// go up branch to calculate real cumultive score to this point
    double scores[NUMBER_OF_TEAMS];
    for (int i = 0; i < NUMBER_OF_TEAMS; i++) scores[i] = 0;
	double total = 0.0;
	const Node* n = from_node;
    int p = sim_search_context->pick - 1;
	while (n->parent != NULL)
	{
        scores[team_with_pick(p)] += n->chosen_player->projected_points;
		n = n->parent;
        p--;
	}

	while (sim_search_context->pick < NUMBER_OF_PICKS)
	{
		const PlayerRecord* player = sim_pick_for_team(sim_search_context, config);
		assert(player != NULL);

        scores[team_with_pick(sim_search_context->pick)] += player->projected_points;
        total += player->projected_points;

		make_pick(sim_search_context, player, config);
	}

    // Calculate score share from sums
    double score_share = scores[drafting_team] / total;

	destroy_search_context(sim_search_context);
	return score_share;
}

static const PlayerRecord* sim_pick_for_team(const SearchContext* context, const DraftConfig* config)
{
    // TODO: Can actually do the random number first so we aren't wasting time doing picking
    // functions that we won't use. i.e if random_number == 0 do zscore etc. etc.
    int rand_index = random() % 3;

    switch (rand_index)
    {
        case 0:
            return zscore_pick_method(context, config);
        case 1:
            return greedy_pick_method(context, config);
        case 2:
        default:
            return random_pick_method(context, config);
    }
}

// Chooses the player with the highest z-score. Used to represent the best "value" pick.
static const PlayerRecord* zscore_pick_method(const SearchContext* context, const DraftConfig* config)
{
    const PlayerRecord* picked_player = NULL;
	const int* const still_required = context->team_requirements[team_with_pick(context->pick)];

    double max_zscore = 0.0;
    for (int i = 0; i < NUMBER_OF_SLOTS; i++)
    {
        const Slot* slot = &config->slots[i];
        const PlayerRecord* player = whos_highest_projected(slot, context->taken, context->pick, config);
        if (player && still_required[i] > 0)
        {
            double zscore = zscores[player->id];
            if (!picked_player || zscore > max_zscore)
            {
                picked_player = player;
                max_zscore = zscore;
            }
        }
    }

    return picked_player;
}

// Greedily chooses the draftable player with the highest projected points.
static const PlayerRecord* greedy_pick_method(const SearchContext* context, const DraftConfig* config)
{
    const PlayerRecord* picked_player = NULL;
	const int* const still_required = context->team_requirements[team_with_pick(context->pick)];

    double max_score = 0.0;
    for (int i = 0; i < NUMBER_OF_SLOTS; i++)
    {
        const Slot* slot = &config->slots[i];
        const PlayerRecord* player = whos_highest_projected(slot, context->taken, context->pick, config);
        if (player && still_required[i] > 0)
        {
            if (!picked_player || player->projected_points > max_score)
            {
                picked_player = player;
                max_score = player->projected_points;
            }
        }
    }

    return picked_player;
}

// Randomly selects an valid postion to draft and selects the highest projected player at that position.
// Used to add randomness and potentially discover new, better draft routes in combination with Monte Carlo.
static const PlayerRecord* random_pick_method(const SearchContext* context, const DraftConfig* config)
{
	const int* const still_required = context->team_requirements[team_with_pick(context->pick)];

	const PlayerRecord* list[config->num_slots];
    int len = 0;
    for (int i = 0; i < config->num_slots; i++)
    {
        const Slot* slot = &config->slots[i];
        const PlayerRecord* player = whos_highest_projected(slot, context->taken, context->pick, config);
        if (player && still_required[i] > 0)
        {
            list[len++] = player;
        }
    }
    if (len == 0)
    {
        return NULL;
    }
    int rand_index = random() % len;
    return list[rand_index];
}

// I am taking the approach of each node's score being a average of all
// the playouts that have been run with this node. I do not know if this is
// the correct approach. I am experimenting with it. To me, the highest average
// would correspond to a better "probability" of winning the tree should you
// take that node. Just taking the highest score could lead to the algorithm
// favoring a path simply because the simulation phase got a lucky, but improbable, 
// rollout for the drafting team.
static void backpropogate_score(Node* node, double score, int team)
{
	if (!node)
		return;
    // Calculate the node's new average score.
    // Subtract 1 from visited because the node's visited has been incremented
    // before it has had any score assigned to it.
    double total = (node->visited-1) * node->scores[team];
    double new_avg =  score + total / node->visited;
	node->scores[team] = new_avg;
	backpropogate_score(node->parent, new_avg, team);
}

static void calculate_zscores(const DraftConfig* config)
{
	//TODO: Figure out what to do on FLEX positions.
	//TODO: Fix PlayerRecord iterators so we can just do double loop.
	// Find the "draftable pool", meaning TODO (finish comment cuz this be important)
    int* NUM_DRAFT_POOL = malloc(sizeof(int) * NUMBER_OF_SLOTS);
	for (int i = 0; i < NUMBER_OF_SLOTS; i++)
	{
		NUM_DRAFT_POOL[i] = config->slots[i].num_required * NUMBER_OF_TEAMS;
	}
    
    for (int i = 0; i < number_of_players; i++)
    {
        const PlayerRecord* player = get_player_by_id(i);
        int pool_size = NUM_DRAFT_POOL[player->position];
        // calculate mean
        double sum = 0.0;
        int n = 0;
        for (const PlayerRecord* p = players_begin(); p != players_end(); p = players_next())
        {
            if (p->position == player->position)
            {
                sum += p->projected_points;
                n++;
            }
            if (n >= pool_size)
            {
                break;
            }
        }
        double mean = sum / (double)n;

        // calculate standard deviation
        int j = 0;
        double sum_of_squares = 0.0;
        for (const PlayerRecord* p = players_begin(); p != players_end(); p = players_next())
        {
            if (p->position == player->position)
            {
                sum_of_squares += pow(fabs(p->projected_points - mean), 2.0);
                j++;
            }
            if (j >= pool_size)
            {
                break;
            }
        }
        double stddev = sqrt(sum_of_squares / n);

        zscores[i] = (player->projected_points - mean) / stddev;
    }
	free(NUM_DRAFT_POOL);
}

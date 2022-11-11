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

// Globals which are set by the corresponding values
// in the passed DraftConfig to calculate_best_pick.
// These values get set at the beginning of calculate_best_pick and
// SHOULD NOT BE MODIFIED.
static int NUMBER_OF_TEAMS = 0;
static int NUMBER_OF_SLOTS = 0;
static int NUMBER_OF_PICKS = 0;

// Zscores get calculated and stashed in this array at beginning 
// of calculate_best_pick.
static double zscores[1000];

typedef struct Node
{
    int visited;
    double* scores;
    const PlayerRecord* chosen_player;
    Node* parent;
	Node* children;
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
	// int team_requirements[NUMBER_OF_TEAMS][NUM_POSITIONS];
} SearchContext;

static Node* create_node(Node* parent, const PlayerRecord* chosen_player);
static void free_node(Node *node);
static SearchContext* create_search_context(int pick, const Taken* taken);
static void destroy_search_context(SearchContext* context);
static void reset_search_context_to(const SearchContext* original, SearchContext* delta);
static Node* select_child(const Node* parent, int team);
static double calculate_ucb(const Node* node, int team);
static bool is_leaf(const Node* node);
static void make_pick(SearchContext* context, const PlayerRecord* player);
static void expand_tree(Node* node, const SearchContext* const context);
static double simulate_score(const SearchContext* context, const Node* from_node);
static const PlayerRecord* sim_pick_for_team(const SearchContext* const context);
static void backpropogate_score(Node* node, double score, int team);
static void calculate_zscores(void);


// Uses the Monte Carlo Tree Search Algorithm to find which available player 
// maximizes the teams total projected fantasy points.
//
// Uses projected_points z_score as a heuristic for simulation pick.
//
// @param thinking_time: Time in seconds the algorithm has before it returns an answer
// @param pick: Initializes the search to think we are at this pick number
// @param taken: Initializes the search to think these players are taken
// @param draft_config: Specifies the slots and number_of_teams in draft
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
    calculate_zscores();

	srand(time(NULL));
    clock_t start_time_s = clock() / CLOCKS_PER_SEC;
    if (start_time_s < 0)
    {
        return NULL;
    }

	// MASTER_CONTEXT reflects the real state of the draft i.e Actual current pick in the draft and
	// actual taken players outside of this function.
	SearchContext* MASTER_CONTEXT = create_search_context(pick, taken);
	SearchContext* current_context = create_search_context(pick, taken);

    Node* root = create_node(NULL, NULL);

	MASTER_CONTEXT->node = root;
	current_context->node = root;
    int max_depth = 0;
    while ( (clock() / CLOCKS_PER_SEC) - start_time_s < thinking_time )
    {
        Node* node = current_context->node;
        if (!node) // We are out of players to pick
        {
            break;
        }
        if (node->parent != NULL)
        {
            make_pick(current_context, node->chosen_player);
            node->visited++;
        }
        if (is_leaf(node))
        {
            expand_tree(node, current_context);
            if (node->parent != NULL) // We don't calculate score for root
            {
                current_context->pick--;
                double score = simulate_score(current_context, node);
                if (current_context->pick > max_depth)
                {
                    max_depth = current_context->pick;
                }
                // Also increments node's "visited" member as score back-propogates
                backpropogate_score(node, score, team_with_pick(current_context->pick)); 
            }
			reset_search_context_to(MASTER_CONTEXT, current_context);
        }
        else
        {
            current_context->node = select_child(node, team_with_pick(current_context->pick));
        }
    } 

	// Destroying context removes node so we have to copy which player
	// it would have taken.
	PlayerRecord* const chosen_player = malloc(sizeof(PlayerRecord));
	double max = 0.0;
	int child = 0;
	int team = team_with_pick(pick);
	for (int i = 0; i < NUM_POSITIONS; i++)
	{
		if (root->children[i] && root->children[i]->score[team] > max)
		{
			max = root->children[i]->score[team];
			child = i;
		}
	}
	memcpy(chosen_player, root->children[child]->chosen_player, sizeof(PlayerRecord));

	destroy_search_context(MASTER_CONTEXT);
	destroy_search_context(current_context);

	free_node(root);

    return chosen_player;
}

static Node* create_node(Node* parent, const PlayerRecord* chosen_player)
{
    Node* node = malloc(sizeof(Node));

    node->parent = parent;
    node->visited = 0;
    node->chosen_player = chosen_player;
    node->scores = malloc(sizeof(double) * NUMBER_OF_TEAMS);
    node->children = malloc(sizeof(Node) * NUMBER_OF_SLOTS);

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

static SearchContext* create_search_context(int pick, const Taken* taken)
{
	SearchContext* context = malloc(sizeof(SearchContext) + sizeof(int*) * NUMBER_OF_TEAMS);
	context->node = NULL;
	context->pick = pick;
    context->taken = malloc(sizeof(Taken) * NUMBER_OF_PICKS);
	memcpy(context->taken, taken, NUMBER_OF_PICKS * sizeof(Taken));
	for (int i = 0; i < NUMBER_OF_TEAMS; i++)
	{
        context->team_requirements[i] = malloc(sizeof(int) * NUMBER_OF_SLOTS);
		context->team_requirements[i][QB] = NUMBER_OF_QB;
		context->team_requirements[i][RB] = NUMBER_OF_RB;
		context->team_requirements[i][WR] = NUMBER_OF_WR;
		context->team_requirements[i][TE] = NUMBER_OF_TE;
		context->team_requirements[i][FLEX] = NUMBER_OF_FLEX;
		context->team_requirements[i][K] = NUMBER_OF_K;
		context->team_requirements[i][DST] = NUMBER_OF_DST;
	}
	// Loop through taken and mark off those players from team_requirements
	// Note: An oddity is that we first decrement FLEX for RB/WR as it makes
	// it easier to handle team position requirements.
	// TODO: Duplication here and in make_picks looks like code smell.
	for (int i = 0; i < pick; i++) 
	{
		Taken t = taken[i];
		const PlayerRecord* player = get_player_by_id(t.player_id);
        if (context->team_requirements[t.by_team][player->position] > 0)
        {
			context->team_requirements[t.by_team][player->position]--;
        }
        else if ((player->position == RB || player->position == WR) && (context->team_requirements[t.by_team][FLEX] > 0)) {
			context->team_requirements[t.by_team][FLEX]--;
		}
	}
	
	return context;
}

static void destroy_search_context(SearchContext* context)
{
	free(context);
	context = NULL;
}

static void reset_search_context_to(const SearchContext* original, SearchContext* delta)
{
	delta->node = original->node;
	delta->pick = original->pick;
	memcpy(delta->taken, original->taken, NUMBER_OF_PICKS * sizeof(Taken));
	memcpy(delta->team_requirements, original->team_requirements, NUMBER_OF_TEAMS * NUM_POSITIONS * sizeof(int));
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

static void make_pick(SearchContext* context, const PlayerRecord* player)
{
	int team = team_with_pick(context->pick);
	context->taken[context->pick].player_id = player->id;
	context->taken[context->pick].by_team = team;

    if (context->team_requirements[team][player->position] > 0)
    {
		context->team_requirements[team][player->position]--;
    }
    else if ((player->position == RB || player->position == WR) && (context->team_requirements[team][FLEX] > 0))
	{
		context->team_requirements[team][FLEX]--;
	}

    context->pick++;
}

static void expand_tree(Node* const node, const SearchContext* const context)
{
	assert(node != NULL);
    int pick = context->pick;
	const int* requirements = context->team_requirements[team_with_pick(pick)];

	const PlayerRecord* qb = whos_highest_projected(QB, context->taken, pick);	
	node->children[QB] = (qb != NULL && requirements[QB] > 0) ? create_node(node, qb) : NULL;

	const PlayerRecord* rb = whos_highest_projected(RB, context->taken, pick);	
	node->children[RB] = (rb != NULL && requirements[RB] > 0) ? create_node(node, rb) : NULL;

	const PlayerRecord* wr = whos_highest_projected(WR, context->taken, pick);	
	node->children[WR] = (wr != NULL && requirements[WR] > 0) ? create_node(node, wr) : NULL;

	const PlayerRecord* te = whos_highest_projected(TE, context->taken, pick);	
	node->children[TE] = (te != NULL && requirements[TE] > 0) ? create_node(node, te) : NULL;

	const PlayerRecord* k = whos_highest_projected(K, context->taken, pick);	
	node->children[K] = (k != NULL && requirements[K] > 0) ? create_node(node, k) : NULL;

	const PlayerRecord* dst = whos_highest_projected(DST, context->taken, pick);	
	node->children[DST] = (dst != NULL && requirements[DST] > 0) ? create_node(node, dst) : NULL;

	const PlayerRecord* flex = whos_highest_projected(FLEX, context->taken, pick);
    node->children[FLEX] = (flex != NULL && requirements[FLEX] > 0) ? create_node(node, flex) : NULL;
}

static double simulate_score(const SearchContext* context, const Node* from_node)
{
	// Copy search context so we can simulate in isolation
	SearchContext* sim_search_context = create_search_context(context->pick, context->taken);
	reset_search_context_to(context, sim_search_context);

	unsigned int drafting_team = team_with_pick(context->pick);

	// Assume pick from from_node happened and sim remaining rounds
	make_pick(sim_search_context, from_node->chosen_player);

	// go up branch to calculate real cumultive score to this point
	double score = 0.0;
	const Node* n = from_node;
    int p = sim_search_context->pick - 1;
	while (n->parent != NULL)
	{
        if (team_with_pick(p) == drafting_team) 
        {
		    score += n->chosen_player->projected_points;
        }
		n = n->parent;
        p--;
	}

	while (sim_search_context->pick < NUMBER_OF_PICKS)
	{
		const PlayerRecord* player = sim_pick_for_team(sim_search_context);
        //TODO: I inserted this cheap return score thing because the assert below was 
        // triggering... figure out WHY. I suspect its because we are running out of players or
        // something???
        if (!player)
        {
            return score;
        }
		assert(player != NULL);

		// Only count score for every cycle of picks so
		// we add up score of a single team.
		if (team_with_pick(sim_search_context->pick) == drafting_team) 
		{
			score += player->projected_points;
		}

		make_pick(sim_search_context, player);
	}

	destroy_search_context(sim_search_context);
	return score;
}

static const PlayerRecord* sim_pick_for_team(const SearchContext* const context)
{
	const Taken* const sim_taken = context->taken;
	const int* const still_required = context->team_requirements[team_with_pick(context->pick)];

    const PlayerRecord* picked_player = NULL;
    double max_zscore = 0.0;
    for (int i = 0; i < NUMBER_OF_SLOTS; i++)
    {
        const PlayerRecord* player = whos_highest_projected(i, sim_taken, context->pick);
        if (player && still_required[i] > 0)
        {
            double zscore = zscores[player->id];
            if (zscore > max_zscore)
            {
                picked_player = player;
                max_zscore = zscore;
            }
        }
    }
    return picked_player;
}

static void backpropogate_score(Node* node, double score, int team)
{
	if (!node)
		return;
	if (score > node->scores[team])
	{
		node->scores[team] = score;
	}
	node->visited++;
	backpropogate_score(node->parent, score, team);
}

static void calculate_zscores(void)
{
    int NUM_DRAFT_POOL[NUM_POSITIONS];
    NUM_DRAFT_POOL[QB] = NUMBER_OF_QB * NUMBER_OF_TEAMS;
    // Add NUMBER_OF_FLEX to position because no player actually has "FLEX" as position. But
    // we still need to represent those players in the draft "pool"
    NUM_DRAFT_POOL[RB] = (NUMBER_OF_RB + NUMBER_OF_FLEX) * NUMBER_OF_TEAMS;
    NUM_DRAFT_POOL[WR] = (NUMBER_OF_WR + NUMBER_OF_FLEX) * NUMBER_OF_TEAMS;
    NUM_DRAFT_POOL[TE] = NUMBER_OF_TE * NUMBER_OF_TEAMS;
    NUM_DRAFT_POOL[FLEX] = NUMBER_OF_FLEX * NUMBER_OF_TEAMS;
    NUM_DRAFT_POOL[K] = NUMBER_OF_K * NUMBER_OF_TEAMS;
    NUM_DRAFT_POOL[DST] = NUMBER_OF_DST * NUMBER_OF_TEAMS;
    
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
}

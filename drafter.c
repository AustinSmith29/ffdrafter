#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "drafter.h"
#include "config.h"

typedef struct Node
{
    int visited;
    double score[NUMBER_OF_TEAMS];
    const struct PlayerRecord* chosen_player;
    struct Node* parent;
	struct Node* children[NUM_POSITIONS];
} Node;

static Node* create_node(Node* parent, const struct PlayerRecord* chosen_player);
static void free_node(Node *node);

typedef struct PositionRequirements {
	int for_team;
	int still_required[NUM_POSITIONS]; 
} PositionRequirements;

// We will be going down "experimental" branches of draft trees
// and will need a way to keep track/reset search state when
// we want to switch to a different branch. This structure
// packs all this context data together.
typedef struct SearchContext
{
	Node* node;
	int pick;
	Taken taken[NUMBER_OF_PICKS];
	PositionRequirements team_requirements[NUMBER_OF_TEAMS];
} SearchContext;

static SearchContext* create_search_context(int pick, const Taken* taken);
SearchContext* create_search_context(int pick, const Taken* taken)
{
	SearchContext* context = malloc(sizeof(SearchContext));
	context->node = NULL;
	context->pick = pick;
	memcpy(context->taken, taken, NUMBER_OF_PICKS * sizeof(Taken));
	for (int i = 0; i < NUMBER_OF_TEAMS; i++)
	{
		context->team_requirements[i].for_team = i;
		context->team_requirements[i].still_required[QB] = NUMBER_OF_QB;
		context->team_requirements[i].still_required[RB] = NUMBER_OF_RB;
		context->team_requirements[i].still_required[WR] = NUMBER_OF_WR;
		context->team_requirements[i].still_required[TE] = NUMBER_OF_TE;
		context->team_requirements[i].still_required[FLEX] = NUMBER_OF_FLEX;
		context->team_requirements[i].still_required[K] = NUMBER_OF_K;
		context->team_requirements[i].still_required[DST] = NUMBER_OF_DST;
	}
	// Loop through taken and mark off those players from team_requirements
	// Note: An oddity is that we first decrement FLEX for RB/WR as it makes
	// it easier to handle team position requirements.
	// TODO: Duplication here and in make_picks looks like code smell.
	for (int i = 0; i < pick; i++) 
	{
		Taken t = taken[i];
		const PlayerRecord* player = get_player_by_id(t.player_id);
		if ((player->position == RB || player->position == WR) && (context->team_requirements[t.by_team].still_required[FLEX] > 0)) {
			context->team_requirements[t.by_team].still_required[FLEX]--;
		}
		else {
			context->team_requirements[t.by_team].still_required[player->position]--;
		}
	}
	
	return context;
}

static void destroy_search_context(SearchContext* context)
{
	free(context);
	context = NULL;
}

static void reset_search_context_to(const SearchContext* original, SearchContext* delta);
static Node* select_child(const Node* parent, int team);
static bool is_leaf(const Node* node);
static void expand_tree(Node* node, const SearchContext* context);
static double simulate_score(const SearchContext* context, const Node* from_node);
static const PlayerRecord* sim_pick_for_team(const SearchContext* context);
static void backpropogate_score(Node* node, double score, int team);

static void make_pick(SearchContext* context, const PlayerRecord* player)
{
	int team = team_with_pick(context->pick);
	context->taken[context->pick].player_id = player->id;
	context->taken[context->pick].by_team = team;

	if ((player->position == RB || player->position == WR) && (context->team_requirements[team].still_required[FLEX] > 0))
	{
		context->team_requirements[team].still_required[FLEX]--;
	}
	else {
		context->team_requirements[team].still_required[player->position]--;
	}
}

// Uses the Monte Carlo Tree Search Algorithm to find which available player 
// maximizes the teams total projected fantasy points.
//
// @param thinking_time: Time in seconds the algorithm has before it returns an answer
// @param pick: Initializes the search to think we are at this pick number
// @param taken: Initializes the search to think these players are taken
const PlayerRecord* calculate_best_pick(int thinking_time, int pick, Taken taken[])
{
	srand(time(NULL));
    clock_t start_time_s = clock() / CLOCKS_PER_SEC;
    if (start_time_s < 0)
        return NULL;

	// Reflects the real state of the draft i.e Actual current pick in the draft and
	// actual taken players outside of this function.
	SearchContext* MASTER_CONTEXT = create_search_context(pick, taken);
	SearchContext* current_context = create_search_context(pick, taken);

    Node* root = create_node(NULL, NULL);
	expand_tree(root, current_context);

	MASTER_CONTEXT->node = root;
	current_context->node = root;
    while ( (clock() / CLOCKS_PER_SEC) - start_time_s < thinking_time )
    {
        Node* node = select_child(current_context->node, team_with_pick(current_context->pick));
		// I think we hit this state when we have expanded to a terminal state so I think we can
		// just reset the search context and continue with algorithm.
		if (!node || current_context->pick >= NUMBER_OF_PICKS) {
			reset_search_context_to(MASTER_CONTEXT, current_context);
			continue;
		}

        if (is_leaf(node)) {
			current_context->pick++;
            expand_tree(node, current_context);
			current_context->pick--;
            double score = simulate_score(current_context, node);
            backpropogate_score(node, score, team_with_pick(current_context->pick)); // Also increments node's "visited" member as score propogates
			reset_search_context_to(MASTER_CONTEXT, current_context);
        }
        else 
		{
			// We move down the tree, so we are exploring a branch where we
			// theoretically draft the player at node, so we have to update
			// the current search context
			node->visited++;
            current_context->node = select_child(node, team_with_pick(current_context->pick));
			// TODO: Investigate WHY this is happening rather than do this hacky solution
			if (!current_context->node)
			{
				reset_search_context_to(MASTER_CONTEXT, current_context);
				continue;
			}
			if (is_leaf(current_context->node)) {
				current_context->node = node;
			}
			make_pick(current_context, node->chosen_player);
			current_context->pick++;
        }
    } 

	// Destroying context removes node so we have to copy which player
	// it would have taken.
	const PlayerRecord* chosen_player = malloc(sizeof(PlayerRecord));
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

void reset_search_context_to(const SearchContext* original, SearchContext* delta)
{
	delta->node = original->node;
	delta->pick = original->pick;
	memcpy(delta->taken, original->taken, NUMBER_OF_PICKS * sizeof(Taken));
	memcpy(delta->team_requirements, original->team_requirements, NUMBER_OF_TEAMS * sizeof(PositionRequirements));
}

static double calculate_ucb(const Node* node, int team);
Node* select_child(const Node* parent, int team)
{
	assert(parent != NULL);
	Node* max_score_node = NULL;
	double max_score = 0.0;
	for (int i = 0; i < NUM_POSITIONS; i++) 
	{
		const Node* child = parent->children[i];
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

double calculate_ucb(const Node* node, int team)
{
	assert(node != NULL);
	double total = 0.0;
	for (int i = 0; i < NUM_POSITIONS; i++) 
	{
		const Node* child = node->children[i];
		if (child)
			total += child->score[team];
	}
	double mean_score = total / NUM_POSITIONS;
	return mean_score + 2 * sqrt(log(node->parent->visited) / node->visited);
}

bool is_leaf(const Node* node)
{
	assert(node != NULL);
	return node->visited == 0;
}

void expand_tree(Node* node, const SearchContext* context)
{
	assert(node != NULL);
	const int* requirements = context->team_requirements[team_with_pick(context->pick)].still_required;

	const PlayerRecord* qb = whos_highest_projected(QB, context->taken, context->pick);	
	node->children[QB] = (qb != NULL && requirements[QB] > 0) ? create_node(node, qb) : NULL;

	const PlayerRecord* rb = whos_highest_projected(RB, context->taken, context->pick);	
	node->children[RB] = (rb != NULL && requirements[RB] > 0) ? create_node(node, rb) : NULL;

	const PlayerRecord* wr = whos_highest_projected(WR, context->taken, context->pick);	
	node->children[WR] = (wr != NULL && requirements[WR] > 0) ? create_node(node, wr) : NULL;

	const PlayerRecord* te = whos_highest_projected(TE, context->taken, context->pick);	
	node->children[TE] = (te != NULL && requirements[TE] > 0) ? create_node(node, te) : NULL;

	const PlayerRecord* k = whos_highest_projected(K, context->taken, context->pick);	
	node->children[K] = (k != NULL && requirements[K] > 0) ? create_node(node, k) : NULL;

	const PlayerRecord* dst = whos_highest_projected(DST, context->taken, context->pick);	
	node->children[DST] = (dst != NULL && requirements[DST] > 0) ? create_node(node, dst) : NULL;

	const PlayerRecord* flex;
	if (wr && !rb)
	{
		flex = wr;
	}
	else if (rb && !wr)
	{
		flex = rb;
	}
	else if (!rb && !wr)
	{
		flex = NULL;
	}
	else 
	{
		flex = (rb->projected_points >= wr->projected_points) ? rb : wr;	
	}
	node->children[FLEX] = (flex != NULL && requirements[FLEX] > 0) ? create_node(node, flex) : NULL;
}

double simulate_score(const SearchContext* context, const Node* from_node)
{
	// Copy search context so we can simulate in isolation
	SearchContext* sim_search_context = create_search_context(context->pick, context->taken);
	reset_search_context_to(context, sim_search_context);

	unsigned int drafting_team = team_with_pick(context->pick);

	// Assume pick from from_node happened and sim remaining rounds
	make_pick(sim_search_context, from_node->chosen_player);
	if (sim_search_context->team_requirements[team_with_pick(sim_search_context->pick)].still_required[from_node->chosen_player->position] < 0) 
		return -10000000;
	sim_search_context->pick++; // Valid pick... advance pick number and sim

	// go up branch to calculate real cumultive score to this point
	double score = 0.0;
	const Node* n = from_node;
	while (n->parent != NULL)
	{
		//TODO: Only total up picks of players from previous drafting team
		score += n->chosen_player->projected_points;
		n = n->parent;
	}

	while (sim_search_context->pick < NUMBER_OF_PICKS)
	{
		const PlayerRecord* player = sim_pick_for_team(sim_search_context);
		assert(player != NULL);

		make_pick(sim_search_context, player);

		// Only count score for every cycle of picks so
		// we add up score of a single team.
		if (team_with_pick(sim_search_context->pick) == drafting_team) 
		{
			score += player->projected_points;
		}

		sim_search_context->pick++;
	}

	destroy_search_context(sim_search_context);
	return score;
}

const PlayerRecord* sim_pick_for_team(const SearchContext* context)
{
	Taken* sim_taken = context->taken;
	int* const still_required = context->team_requirements[team_with_pick(context->pick)].still_required;

	int still_needed = 0;
	for (int i = 0; i < NUM_POSITIONS; i++)
	{
		if (still_required[i] > 0) 
		{
			still_needed++;
		}
	}
	if (still_needed == 0) return NULL;

	const PlayerRecord* qb = whos_highest_projected(QB, sim_taken, context->pick);	
	const PlayerRecord* rb = whos_highest_projected(RB, sim_taken, context->pick);	
	const PlayerRecord* wr = whos_highest_projected(WR, sim_taken, context->pick);	
	const PlayerRecord* te = whos_highest_projected(TE, sim_taken, context->pick);	
	const PlayerRecord* k = whos_highest_projected(K, sim_taken, context->pick);	
	const PlayerRecord* dst = whos_highest_projected(DST, sim_taken, context->pick);	
	const PlayerRecord* flex = (rb->projected_points >= wr->projected_points) ? rb : wr;	

	assert(qb || rb || wr || te || k || dst || flex);

	const PlayerRecord* list[] = {qb, rb, wr, te, k, dst, flex};

	int randIndex = random() % NUM_POSITIONS;
	// Kinda hacky way to do this but it'll work for now I think.
	// Might have to add filtering do the list to exclude positions
	// that have no available players left or that are unneeded by
	// the team.
	int failsafe = 0;
	while (list[randIndex] == NULL || still_required[list[randIndex]->position] <= 0) 
	{
		randIndex = random() % NUM_POSITIONS;
		failsafe++;
		if (failsafe > 1000) {
			printf("Spinning... still_needed=%d\n", still_needed);
		}
	}

	// TEST: So this looks like this assert never gets triggered... so how tf
	// are we still drafting qb's if we are never picking qbs in sim?
	if (still_required[QB] == 0 && list[randIndex]->position == QB) {
		assert(list[randIndex]->position != QB);
	}

	return list[randIndex];
}


void backpropogate_score(Node* node, double score, int team)
{
	if (!node)
		return;
	if (node->score < 0)
		return;
	//TODO: hack way to stop the score from branches that are supposed to be dead from
	// bubbling up. Need to just make the tree have terminal leaves rather than this negative
	// simulated score bullshit.
	if (score < 0) {
		node->score[team] = score;
		node->visited++;
		backpropogate_score(node->parent, 0, team);
		return;
	}
	if (score > node->score[team])
	{
		node->score[team] = score;
	}
	node->visited++;
	backpropogate_score(node->parent, score, team);
}

Node* create_node(Node* parent, const struct PlayerRecord* chosen_player)
{
    Node* node = malloc(sizeof(Node));
    
    node->parent = parent;
    node->visited = 0;
	for (int i = 0; i < NUMBER_OF_TEAMS; i++) 
    	node->score[i] = 0.0;
    node->chosen_player = chosen_player;
    node->children[QB] = node->children[RB] = node->children[WR] = node->children[TE] = \
						 node->children[K] = node->children[DST] = node->children[FLEX] = NULL;

    return node;
}

free_node(Node* node)
{
	if (!node)
		return;
    free_node(node->children[QB]);
    free_node(node->children[RB]);
    free_node(node->children[WR]);
    free_node(node->children[TE]);
    free_node(node->children[K]);
    free_node(node->children[DST]);
    node->children[QB] = node->children[RB] = node->children[WR] = node->children[TE] = \
						 node->children[K] = node->children[DST] = NULL;

    free(node);
    node = NULL;
}

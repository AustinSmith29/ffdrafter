#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "drafter.h"
#include "config.h"

#define TEAM_WITH_PICK(pick) ( (pick) % (NUMBER_OF_TEAMS) )

typedef struct Node
{
    int visited;
    double score;
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
static Node* select_child(const Node* parent);
static bool is_leaf(const Node* node);
static void expand_tree(Node* node, Taken taken[], int passed_picks);
static double simulate_score(const SearchContext* context, const Node* from_node);
static const PlayerRecord* sim_pick_for_team(const SearchContext* context);
static void backpropogate_score(Node* node, double score);

static void make_pick(SearchContext* context, const PlayerRecord* player)
{
	int team = TEAM_WITH_PICK(context->pick);
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


int drafting_team = 0;
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
	drafting_team = TEAM_WITH_PICK(pick);

    Node* root = create_node(NULL, NULL);
	expand_tree(root, current_context->taken, pick);

	MASTER_CONTEXT->node = root;
	current_context->node = root;
    while ( (clock() / CLOCKS_PER_SEC) - start_time_s < thinking_time )
    {
        Node* node = select_child(current_context->node);
		if (!node) {
			continue; // TODO: Hacky af but wtf
		}
		if (node->chosen_player == NULL && node != root) {
			continue; // TODO: Hacky af but wtf
		}
		if (current_context->pick >= NUMBER_OF_PICKS) {
			reset_search_context_to(MASTER_CONTEXT, current_context);
		}
        if (is_leaf(node)) {
            expand_tree(node, current_context->taken, current_context->pick);
            double score = simulate_score(current_context, node);
            backpropogate_score(node, score); // Also increments node's "visited" member as score propogates
			reset_search_context_to(MASTER_CONTEXT, current_context);
        }
        else {
			// We move down the tree, so we are exploring a branch where we
			// theoretically draft the player at node, so we have to update
			// the current search context
			node->visited++;
            current_context->node = select_child(node);
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
	int max = 0;
	int child = 0;
	for (int i = 0; i < NUM_POSITIONS; i++)
	{
		if (root->children[i]->score > max)
		{
			max = root->children[i]->score;
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

static double calculate_ucb(const Node* node);
Node* select_child(const Node* parent)
{
	assert(parent != NULL);
	Node* max_score_node = NULL;
	double max_score = 0.0;
	for (int i = 0; i < NUM_POSITIONS; i++) 
	{
		const Node* child = parent->children[i];
		if (child->visited == 0) 
		{
			max_score_node = child;
			break;
		}
		double score = calculate_ucb(child);
		if (score > max_score)
		{
			max_score = score;
			max_score_node = child;
		}
	}
	return max_score_node;
}

double calculate_ucb(const Node* node)
{
	assert(node != NULL);
	double total = 0.0;
	for (int i = 0; i < NUM_POSITIONS; i++) 
	{
		const Node* child = node->children[i];
		total += child->score;
	}
	double mean_score = total / NUM_POSITIONS;
	return mean_score + 2 * sqrt(log(node->parent->visited) / node->visited);
}

bool is_leaf(const Node* node)
{
	assert(node != NULL);
	return node->visited == 0;
}

void expand_tree(Node* node, Taken taken[], int passed_picks)
{
	assert(node != NULL);

	const PlayerRecord* qb = whos_highest_projected(QB, taken, passed_picks);	
	node->children[QB] = create_node(node, qb);

	const PlayerRecord* rb = whos_highest_projected(RB, taken, passed_picks);	
	node->children[RB] = create_node(node, rb);

	const PlayerRecord* wr = whos_highest_projected(WR, taken, passed_picks);	
	node->children[WR] = create_node(node, wr);

	const PlayerRecord* te = whos_highest_projected(TE, taken, passed_picks);	
	node->children[TE] = create_node(node, te);

	const PlayerRecord* k = whos_highest_projected(K, taken, passed_picks);	
	node->children[K] = create_node(node, k);

	const PlayerRecord* dst = whos_highest_projected(DST, taken, passed_picks);	
	node->children[DST] = create_node(node, dst);

	const PlayerRecord* flex = (rb->projected_points >= wr->projected_points) ? rb : wr;	
	node->children[FLEX] = create_node(node, flex);
}

double simulate_score(const SearchContext* context, const Node* from_node)
{
	// Copy search context so we can simulate in isolation
	SearchContext* sim_search_context = create_search_context(context->pick, context->taken);
	reset_search_context_to(context, sim_search_context);

	// Assume pick from from_node happened and sim remaining rounds
	make_pick(sim_search_context, from_node->chosen_player);
	if (sim_search_context->team_requirements[TEAM_WITH_PICK(sim_search_context->pick)].still_required[from_node->chosen_player->position] < 0) 
		return -10000000;
	sim_search_context->pick++; // Valid pick... advance pick number and sim

	double score = from_node->score + from_node->chosen_player->projected_points;
	//unsigned int drafting_team = TEAM_WITH_PICK(context->pick);

	while (sim_search_context->pick < NUMBER_OF_PICKS-1)
	{
		const PlayerRecord* player = sim_pick_for_team(sim_search_context);
		assert(player != NULL);

		make_pick(sim_search_context, player);

		// Only count score for every cycle of picks so
		// we add up score of a single team.
		if (TEAM_WITH_PICK(sim_search_context->pick) == drafting_team) {
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
	int* const still_required = context->team_requirements[TEAM_WITH_PICK(context->pick)].still_required;

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


void backpropogate_score(Node* node, double score)
{
	if (!node)
		return;
	if (node->score < 0)
		return;
	//TODO: hack way to stop the score from branches that are supposed to be dead from
	// bubbling up. Need to just make the tree have terminal leaves rather than this negative
	// simulated score bullshit.
	if (score < 0) {
		node->score = score;
		node->visited++;
		backpropogate_score(node->parent, 0);
		return;
	}
	if (score > node->score)
	{
		node->score = score;
	}
	node->visited++;
	backpropogate_score(node->parent, score);
}

Node* create_node(Node* parent, const struct PlayerRecord* chosen_player)
{
    Node* node = malloc(sizeof(Node));
    
    node->parent = parent;
    node->visited = 0;
    node->score = 0.0;
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

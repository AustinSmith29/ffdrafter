#include <stdlib.h>
#include <stddef.h>
#include <time.h>

#include "drafter.h"
#include "players.h"

typedef struct Node
{
    int visited;
    double score;
    const PlayerRecord* chosen_player;

    struct Node* parent;
    struct Node* qb;
    struct Node* rb;
    struct Node* wr;
    struct Node* te;
    struct Node* k;
    struct Node* dst;
} Node;

static Node* create_node();
static void free_node(Node *node);

const PlayerRecord* calculate_best_pick(int thinking_time, int taken[])
{
    clock_t start_time_s = clock() / CLOCKS_PER_SEC;
    if (start_time_s < 0)
        return NULL;

    Node* root = create_node(NULL);
    Node* current = root;
    while ( (clock() / CLOCKS_PER_SEC) - start_time_s < thinking_time )
    {
        /*
        Node *node = select_child(current);
        if (is_leaf(node)) {
            expand_tree();
            double score = simulate_score(taken);
            backpropogate_score(score);
            node = root;
        }
        else {
            current = node;
        }
        */

    } 

    return root->chosen_player;
}

Node* create_node(Node* parent)
{
    Node* node = malloc(sizeof(Node));
    
    node->parent = parent;
    node->visited = 0;
    node->score = 0.0;
    node->chosen_player = NULL;
    node->qb = node->rb = node->wr = node->te = node->k = node->dst = NULL;

    return node;
}

free_node(Node* node)
{
    free_node(node->qb);
    free_node(node->rb);
    free_node(node->wr);
    free_node(node->te);
    free_node(node->k);
    free_node(node->dst);
    node->qb = node->rb = node->wr = node->te = node->k = node->dst = node->parent = NULL;

    free(node);
    node = NULL;
}

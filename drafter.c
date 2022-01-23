#include <stdlib.h>
#include <stddef.h>
#include <time.h>

#include "drafter.h"

typedef struct Node
{
    int visited;
    double score;
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

    Node* root = create_node();
    while ( (clock() / CLOCKS_PER_SEC) - start_time_s < thinking_time )
    {

    } 

    return root;
}

Node* create_node()
{
    Node* node = malloc(sizeof(Node));
    
    node->visited = 0;
    node->score = 0.0;
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
    node->qb = node->rb = node->wr = node->te = node->k = node->dst = NULL;

    free(node);
    node = NULL;
}

#ifndef DRAFTER_H
#define DRAFTER_H

#include "players.h"
#include "config.h"

// Returns the player that the engine thinks will maximize the team's fantasy points.
//
// @param thinking_time: Time in seconds the algorithm has before it returns an answer
// @param pick: Initializes the search to think we are at this pick number
// @param taken: Initializes the search to think these players are taken
// @param draft_config: Specifies the slots and number_of_teams in draft
const PlayerRecord* calculate_best_pick(
    int thinking_time, 
    int pick, 
    Taken taken[], 
    const DraftConfig* draft_config
);

#endif

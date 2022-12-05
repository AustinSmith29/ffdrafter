#ifndef DRAFTER_H
#define DRAFTER_H

#include "players.h"
#include "config.h"

const PlayerRecord* calculate_best_pick(
    int thinking_time, 
    int pick, 
    Taken taken[], 
    const DraftConfig* draft_config
);

#endif

#ifndef PLAYERS_H
#define PLAYERS_H

#define QB   0
#define RB   1
#define WR   2
#define TE   3
#define K    4
#define DST  5
#define FLEX 6

#define NUM_POSITIONS 7

typedef struct PlayerRecord {
	unsigned int id;
	double projected_points;
	int position;
	char* name;
} PlayerRecord;

// Allocates memory and initializes players. Returns number of records loaded into players from csv_file.
// Returns -1 on failure.
int load_players(const char* csv_file);

void unload_players();

typedef struct Taken {
	unsigned int player_id;
	unsigned int by_team; // index into DRAFT_ORDER array in config.h
} Taken;

int is_taken(int player_id, const Taken taken[], int passed_picks);

// Gets record of the player with the highest projected points at the given position who's id is NOT in
// the taken list.
const PlayerRecord* whos_highest_projected(int position, const Taken taken[], int passed_picks);

const PlayerRecord* get_player_by_id(unsigned int player_id);
const PlayerRecord* get_player_by_name(const char* name);

// Constant Iterators
const PlayerRecord* players_begin();
const PlayerRecord* players_end();
const PlayerRecord* players_next();

#endif

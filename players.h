#ifndef PLAYERS_H
#define PLAYERS_H

#define QB   0
#define RB   1
#define WR   2
#define TE   3
#define K    4
#define DST  5
#define FLEX 6

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

// Gets record of the player with the highest projected points at the given position who's id is NOT in
// the taken list.
const PlayerRecord* whos_highest_projected(char position, unsigned int taken[]);

// Constant Iterators
const PlayerRecord* players_begin();
const PlayerRecord* players_end();
const PlayerRecord* players_next();

#endif

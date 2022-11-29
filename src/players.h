#ifndef PLAYERS_H
#define PLAYERS_H

extern int number_of_players;

typedef struct PlayerRecord {
	unsigned int id;
	double projected_points;
	int position;
	char* name;
} PlayerRecord;

struct DraftConfig;
struct Slot;

// Allocates memory and initializes players. Returns number of records loaded into players from csv_file.
// Returns -1 on failure.
int load_players(const char* csv_file, const struct DraftConfig* config);

void unload_players();

typedef struct Taken {
	unsigned int player_id;
	unsigned int by_team; // index into DRAFT_ORDER array in config.h
} Taken;

int is_taken(int player_id, const Taken taken[], int passed_picks);

// Gets record of the player with the highest projected points at the given slot who's id is NOT in
// the taken list.
const PlayerRecord* whos_highest_projected(const struct Slot* slot, const Taken taken[], int passed_picks);

const PlayerRecord* get_player_by_id(unsigned int player_id);
const PlayerRecord* get_player_by_name(const char* name);

// Constant Iterators
const PlayerRecord* players_begin();
const PlayerRecord* players_end();
const PlayerRecord* players_next();

#endif

#define QB   0
#define RB   1
#define WR   2
#define TE   3
#define K    4
#define DST  5
#define FLEX 6

static PlayerRecord* players; // can i do this???

typedef struct PlayerRecord {
	int id;
	float projected_points;
	char position;
	char* name;
	char* team;
} PlayerRecord;

// Returns number of records loaded into players from csv_file
int load_players(PlayerRecord* players, const char* csv_file);

const PlayerRecord* whos_highest_projected(char position);

// DRAFT CONFIGURATION CONSTANTS (edit this and recompile to change settings)
// =================================================================================================
const char* PLAYER_CSV_LIST = "projections_2021.csv"; // file that has Player Names, Positions, and Projected Points

const int NUMBER_OF_TEAMS = 12;
const int NUMBER_OF_QBS = 1;
const int NUMBER_OF_RB = 2;
const int NUMBER_OF_WR = 2;
const int NUMBER_OF_TE = 1;
const int NUMBER_OF_FLEX = 2;
const int NUMBER_OF_K = 0;
const int NUMBER_OF_DST = 0;

// Sets order of first round of draft. Subsequent rounds are determined via snake draft.
// If you want the computer to make the optimal pick for this player, set the Controller as "AI".
// If the user wants to make the pick for that player, set the Controller as "HUMAN".
const char* DRAFT_ORDER[NUMBER_OF_TEAMS][2] = {
//  {Team Name, Controller}
    {"Nick Scardina", "AI"},
    {"Connor Hetterich", "AI"},
    {"Richard Kroupa", "AI"}
    {"Matt Everhart", "AI"},
    {"Austin Smith", "AI"},
    {"Christian Photos", "AI"},
    {"Daniel Rocco", "AI"},
    {"Alex Scardina", "AI"},
    {"Tyler Strong", "AI"},
    {"Liam Bramley", "AI"},
    {"Steven Sbash", "AI"},
    {"Cameron Urfer", "AI"}
};

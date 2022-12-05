# fDraft
fDraft is an engine to make optimal fantasy picks to maximize the amount of fantasy points
for a team.

## Engine Control
fDraft communicates through standard input. Commands that accept arguments are split using `;`.


Before any commands can be run, with the exception of `exit`, a configuration file must be loaded
into the engine using the `load_draft_config` command. After a configuration file has been loaded,
the engine requires the player pool to be loaded with `load_player_pool`. After that, the engine
is ready to start calculating picks.

Command | Description
--- | ---
 `think_pick` | Returns the player the engine thinks is the optimal pick in the current draft state.
 `make_pick;[player_name]` | Adds the player to the roster of the team currently picking.
 `undo_pick` | Resets the draft state to the previous pick.
 `load_draft_config;[filename]` | loads a draft config. Resets draft state to start.
 `load_player_pool;[filename]` | sets the draft pool. Resets draft state to start.
 `set_think_time;[seconds]` | sets the amount of time the engine will think (in seconds). 
 `state` | Prints current pick number, drafting team, and engine think time.
 `history` | Prints out all the picks that were made so far.
 `roster;[team_id]` | Shows roster slots and summation of fantasy points for team with team_id.
 `available;[position];lim` | Shows  up to lim available players at a position.
 `bench;[player_name]` | Adds player to the roster without counting points toward total.
 `load:[filename]` | loads and resumes draft that was previously started
 `save;[filename]` | saves draft
 `sim_draft` | Engine makes every pick for the remainder of the draft.
 `exit` | Exit out of the engine.

## Configuration
### Player Pool
Players are read from a csv file. Each row is required to have a name, position, and projected points field. There is no header row.

### Draft Configuration
The parameters for the draft are set by a configuration file that gets loaded by the engine via
a command. The configuration file is a libconfig file that has the following shape:
```
number_of_teams = 12;
slots = (
    { 
        name = "QB";
        number_required = 1;
    },
    { 
        name = "RB";
        number_required = 2;
    },
    { 
        name = "WR";
        number_required = 2;
    },
    { 
        name = "TE";
        number_required = 1;
    },
    { 
        name = "FLEX";
        number_required = 2;
        flex_slots = ["RB", "WR", "TE"];
    },
);
```
'Slots' are simply positions that need to be filled up to constitute a complete roster. You
specify the name of the slot (the position) and the number of required players at that position.
There is a special type of slot that allows players from multiple positions to fill it. In the
above example, this special slot is called "FLEX". This slot is differentiated by defining 
an `flex_slots` property, which defines the other slot positions the slot can fill. Each name given 
in the options property must match one of the names of the other defined slots. Also, flex slots must
be defined after all the 'real' slots, as the config parser reads the slots from top to bottom and the
`flex_slots` must reference defined slots.

## Installation
From the root directory of this project: `make`.

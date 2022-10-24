# FFDrafter
FFDrafter is an engine to make optimal fantasy picks to maximize the amount of fantasy points
for a team.

## Usage
ffdrafter communicates through standard input. Commands that accept arguments are split using ';'.
* `think_pick` --> Returns the player the engine thinks is the optimal pick in the current draft state.
* `make_pick;[player_name]` --> Adds the player to the roster of the team currently picking.
* `undo_pick` --> Resets the draft state to the previous pick.
* `set_think_time;[time_in_seconds]` --> sets the amount of time the bot will think for. 
* `state` --> Prints current pick number, drafting team, and bot think time.
* `history` --> Prints out all the picks that were made so far.
* `roster;[team_id]` --> Shows roster slots and summation of fantasy points for team with team_id.
* `pool;[position];lim` --> Shows  up to lim available players at a position.
* `load:[filename]` --> loads and resumes draft that was previously started
* `save;[filename]` --> saves draft
* `sim_draft` --> Engine makes every pick for the remainder of the draft.
* `exit` --> Exit out of the engine.

## Configuration
### Player Pool
Players are read from a csv file. Each row is required to have a name, position, and projected points field. There is no header row.

## Installation
From the root directory of this project: `make`.

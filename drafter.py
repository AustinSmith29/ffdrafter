import re
import math
import random

required_pos = {
    'QB': 1,
    'RB': 2,
    'WR': 2,
    'TE': 1,
    'FLEX': 2,
    'K': 0,
    'DST': 0,
}

def get_snake_picks(num_teams, rounds):
    picks = []
    snake = False
    for r in range(rounds):
        order = range(num_teams) if not snake else range(num_teams)[::-1]
        for team in order:
            picks.append((team, r))
        snake = not snake
    return picks

def get_draftboard():
    draftboard = {}
    with open('projections_2021.csv') as f:
        for line in f:
            rank, name, pos, points = line.strip().split(',')
            pos = re.match('[A-Z]+', pos).group()
            draftboard[int(rank)] = {
                'name': name,
                'pos': pos,
                'points': float(points),
                'rank': int(rank),
                'drafted_by': None
            }
    return draftboard

def get_available_at_pos(pos, draftboard, taken):
    players = []
    for rank, player in draftboard.items():
        if rank in taken: continue
        if player['pos'] == pos:
            players.append(player)
    return players


def get_best_available_at_pos(pos, draftboard, taken):
    available = get_available_at_pos(pos, draftboard, taken)
    if not available:
        return None
    else:
        # draftboard is already sorted in descending order of projected points
        return available[0]

def pick_best_player_at_pos(pos, draftboard, taken, team):
    player = get_best_available_at_pos(pos, draftboard, taken)
    if not player:
        return None
    player['drafted_by'] = team
    return player

def create_teams(num_teams):
    teams = {}
    for i in range(num_teams):
        teams[i] = {'name': '', 'players': []}
    return teams

def ucb(node):
    vi = sum([child.score for child in node.children]) / len(node.children) if len(node.children) > 0 else 0
    return vi + (2 * math.sqrt(math.log(node.parent.visited) / node.visited))

class Node:
    def __init__(self, draftboard, taken, team, parent):
        qb = pick_best_player_at_pos('QB', draftboard, taken, team)
        rb = pick_best_player_at_pos('RB', draftboard, taken, team)
        wr = pick_best_player_at_pos('WR', draftboard, taken, team)
        te = pick_best_player_at_pos('TE', draftboard, taken, team)
        flex = max([rb, wr, te], key=lambda x: x['points'])
        k = pick_best_player_at_pos('K', draftboard, taken, team)
        dst = pick_best_player_at_pos('DST', draftboard, taken, team)
        self.score = 0
        self.visited = 0
        self.taken = []
        self.team = []
        self.parent = parent
        next_pick = None
        self.children = [
            Node(draftboard, self.taken + [qb], next_pick, parent),
            Node(draftboard, self.taken + [rb], next_pick, parent),
            Node(draftboard, self.taken + [wr], next_pick, parent),
            Node(draftboard, self.taken + [te], next_pick, parent),
            Node(draftboard, self.taken + [flex], next_pick, parent),
            Node(draftboard, self.taken + [k], next_pick, parent),
            Node(draftboard, self.taken + [dst], next_pick, parent),
        ]

def is_terminal(node):
    pass

def rollout(node):
    while not is_terminal(node):
        action = random.choice(node.children)

if __name__ == '__main__':
    NUM_TEAMS = 10
    ITERATIONS = 1000
    draftboard = get_draftboard()
    teams = create_teams(NUM_TEAMS)
    picks = get_snake_picks(NUM_TEAMS, 9)

    root = Node(draftboard, [], 0, None)
    for i in range(ITERATIONS):
       # rollout 
       while not is_terminal(current_node):


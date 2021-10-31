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
    total = 0
    for r in range(rounds):
        order = range(num_teams) if not snake else range(num_teams)[::-1]
        for team in order:
            picks.append((total, team, round))
            total += 1
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

def calculate_ucb(node):
    if node.visited == 0:
        return None
    vi = sum([child.score for child in node.children]) / len(node.children) if len(node.children) > 0 else 0
    return vi + (2 * math.sqrt(math.log(node.parent.visited) / node.visited))

class Node:
    def __init__(self, draftboard, taken, pick, parent):
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
        self.team = team
        self.team_rosters = {} 
        self.parent = parent
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
    for team in node.team_rosters:
        required = required_pos.copy()
        for player in team:
            required[player.pos] -= 1
        if required.values().count(0) != len(required_pos.keys()):
            return False
    return True

def rollout(node, draftboard):
    picks = get_snake_picks(10, 8)
    last_pick = picks[-1][0]
    reward = 0
    simming_team = node.team
    team = simming_team
    while node.pick <= last_pick: 
        picking_team = picks[node.pick][1]
        player = random.choice(node.children)
        if picking_team == simming_team:
            reward += player.points
        node = Node(draftboard, node.taken + [player], node.pick+1, node)
    return reward

def choose_child(children):
    for child in children:
        ucb = calculate_ucb(child)
        if not ucb:
            chosen_node = child
            break
        else:
            if ucb > max_usb:
                max_ucb = ucb
                chosen_node = child
    return chosen_node

def backpropogate_value(node, value):
    if not node:
        return
    if value > node.value:
        node.value = value
        backpropogate_value(node.parent, value)

if __name__ == '__main__':
    NUM_TEAMS = 10
    ITERATIONS = 1000
    draftboard = get_draftboard()
    teams = create_teams(NUM_TEAMS)
    picks = get_snake_picks(NUM_TEAMS, 8)
    # print (picks)
    node = Node(draftboard, [], picks[0][0], None)
    for i in range(ITERATIONS):
        node_to_explore = choose_child(node.children)
        if is_leaf(node_to_explore):
            rollout_value = rollout(node_to_explore, draftboard)
            backpropogate_value(node_to_explore, rollout_value)
        else:
            # Add new child node with populated actions
            # and then roll out from it
            pass

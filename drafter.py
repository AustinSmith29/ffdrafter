import re
import math
import random
import copy
import time

def get_snake_picks(num_teams, rounds):
    picks = []
    snake = False
    total = 0
    for r in range(rounds):
        order = range(num_teams) if not snake else range(num_teams)[::-1]
        for team in order:
            picks.append((total, team, r))
            total += 1
        snake = not snake
    return picks

required_pos = {
    'QB': 1,
    'RB': 2,
    'WR': 2,
    'TE': 1,
    'FLEX': 2,
    'K': 0,
    'DST': 0,
}

NUM_TEAMS = 12
ROUNDS = 8
picks = get_snake_picks(NUM_TEAMS, ROUNDS)

def get_draftboard():
    draftboard = {}
    with open('projections_2021.csv') as f:
        i = 0
        for line in f:
            name, pos, points = line.strip().split(',')
            pos = re.match('[A-Z]+', pos).group()
            draftboard[i] = {
                'name': name,
                'pos': pos,
                'points': float(points),
                'rank': i
            }
            i += 1
    return draftboard

def get_available_at_pos(pos, draftboard, taken):
    players = []
    for rank, player in draftboard.items():
        if player['pos'] == pos or (pos == 'FLEX' and (player['pos'] == 'WR' or player['pos'] == 'RB')):
            taken_ranks = map(lambda p: p['rank'], taken)
            if rank in taken_ranks: continue
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
    return player

def create_teams(num_teams):
    teams = {}
    for i in range(num_teams):
        teams[i] = {'name': 'Team {}'.format(i), 'players': []}
    return teams

def num_of_pos_on_team(position, team):
    n = 0
    for player in team['players']:
        if player['pos'] == position:
            n += 1
    return n

def calculate_ucb(node):
    if node.visited == 0:
        return None
    vi = sum([child.score for child in node.children]) / len(node.children) if len(node.children) > 0 else 0
    return vi + (2 * math.sqrt(math.log(node.parent.visited) / node.visited))

class Node:
    def __init__(self, taken, parent, pick, teams):
        self.children = []
        self.score = 0
        self.visited = 0
        self.taken = taken
        self.pick = pick
        self.parent = parent
        self.teams = copy.deepcopy(teams)

    def expand_children(self, draftboard):
        pick_number, team, _ = self.pick
        for pos in ['QB', 'RB', 'WR', 'TE', 'K', 'DST']:
            if num_of_pos_on_team(pos, self.teams[team]) == required_pos[pos]:
                continue
            player = pick_best_player_at_pos(pos, draftboard, self.taken, team)
            if player:
                next_pick = picks[pick_number + 1]
                self.children.append(Node(self.taken + [player], self, next_pick, teams))

def is_terminal(node):
    for team in node.team_rosters:
        required = required_pos.copy()
        for player in team:
            required[player.pos] -= 1
        if required.values().count(0) != len(required_pos.keys()):
            return False
    return True

def rollout(node, draftboard):
    last_pick = picks[-1][0]
    reward = node.score
    simming_team = node.pick[1]
    team = simming_team
    while node.pick[0] <= last_pick-1: 
        node.expand_children(draftboard)
        if not node.children:
            break
        picking_team = node.pick[1]
        choice = random.choice(node.children)
        player = choice.taken[-1]
        if picking_team == simming_team:
            reward += player['points']
        node = Node(node.taken + [player], node, picks[node.pick[0]+1], node.teams)
    return reward

def choose_child(children):
    chosen_node = None
    max_usb = 0
    for child in children:
        ucb = calculate_ucb(child)
        if ucb == None:
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
    if value > node.score:
        node.score = value
        backpropogate_value(node.parent, value)

def is_leaf(node):
    return not (True in list(map(lambda x: True if x.visited > 0 else False, node.children)))

def print_team(team):
    print (team)

if __name__ == '__main__':
    draftboard = get_draftboard()
    teams = create_teams(NUM_TEAMS)
    taken = []
    SECONDS_PER_PICK = 30 
    for x in range(len(picks)-1):
        sim_pick = x
        root = Node(taken, None, picks[sim_pick], teams)
        root.visited = 1
        root.expand_children(draftboard)
        current_node = root
        start_time = time.time()
        while time.time() - start_time < SECONDS_PER_PICK:
            node_to_explore = choose_child(current_node.children)
            if not node_to_explore: 
                node_to_explore = root
            if is_leaf(node_to_explore):
                if node_to_explore.visited == 0:
                    rollout_node = copy.deepcopy(node_to_explore)
                    rollout_value = rollout(rollout_node, draftboard)
                    backpropogate_value(node_to_explore, rollout_value)
                    node_to_explore.visited += 1
                    current_node = root
                else:
                    if not sim_pick < len(picks)-2: continue
                    new_node = Node(node_to_explore.taken, node_to_explore, picks[sim_pick + 1], teams)
                    new_node.expand_children(draftboard)
                    current_node = new_node
                    sim_pick += 1
            else:
                current_node = node_to_explore
        # Make pick
        player = sorted(root.children, key=lambda x: x.score)[-1].taken[-1]
        taken.append(player)
        _, team, _round = picks[x]
        teams[team]['players'].append(player)
        print ("Round {}: Team {} picks {} {}".format(_round, team, player['name'], player['pos']))

    for team, value in teams.items():
        print ('Team {}: {}'.format(team, value['players']))

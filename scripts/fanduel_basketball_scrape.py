import json
import sys

players = []
with sys.stdin as f:
    raw = f.read()
    obj = json.loads(raw)
    for player in obj['data']['contest']['draft']['pickOptions']:
        name = player['player']['firstNames'] + ' ' + player['player']['lastName']
        position = player['slatePlayer']['position']
        if player['slatePlayer']['projectedFantasyPointStats']:
            points = player['slatePlayer']['projectedFantasyPointStats']['fantasyPoints']
        else:
            points = 0.0
        players.append((name, position, points))

players = sorted(players, key=lambda x: x[2], reverse=True)
for p in players:
    print (','.join([p[0], p[1], str(p[2])]), file=sys.stdout)

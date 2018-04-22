#!/usr/bin/python
#
# file:        q2test.py
# description: verify debug output from CS 5600 HW2 simulation.
#
# Peter Desnoyers, Northeastern University CCIS, 2011
#

epsilon = 0.0002

import sys
import getopt

try:
    opts, args = getopt.getopt(sys.argv[1:], '', ['verbose', 'fuzz='])
except getopt.GetoptError, err:
    print str(err)
    sys.exit(2)

verbose = False
for o,a in opts:
    if o == '--verbose':
        verbose = True
    if o == '--fuzz':
        epsilon = float(a)

fp = sys.stdin
if args:
    fp = open(args.pop(0), "r")

events = dict()

# DEBUG: %f barber goes to sleep
# DEBUG: %f barber wakes up
# DEBUG: %f customer %d enters shop
# DEBUG: %f customer %d starts haircut
# DEBUG: %f customer %d leaves shop

# first we build up the timeline, containing an event for every DEBUG
# message. Time is quantized to ticks of 'epsilon' long, so events at
# roughly the same time should show up in the same event bin.
#
# events[] is indexed by timestamps, and contains lists of event
# tuples - each list contains all events which took place at the same
# time.
#
stamp_0 = -1
t = 0
timeline = []

for line in fp:
    if not line.startswith('DEBUG'):
        continue
    tokens = line.split()
    stamp = float(tokens[1])
    if stamp > stamp_0 + epsilon:
        t += 1
        timeline.append(t)
        events[t] = []
    stamp_0 = stamp

    if tokens[2] == 'customer':
        tnum = int(tokens[3])
        action = ' '.join(tokens[4:])
        evt = ('c', tnum, action, line.rstrip())
        events[t].append(evt)
    elif tokens[2] == 'barber':
        action = ' '.join(tokens[3:])
        evt = ('b', 0, action, line.rstrip())
        events[t].append(evt)
    else:
        assert False

# this holds the state for each customer - 'O' (outside shop),
# 'L' (sitting in line), 'H' (getting haircut), 'F' (stepped inside
# when full)
#
state = dict()

for i in range(0,10):
    state[i] = 'O'
barber_awake = True
num_in_shop = 0

# validate customer number
#
def checkcustomer(num, line):
    if num < 0 or num > 9:
        print "ERROR: illegal customer number %d (should be 0..9)" % num
        print "---> ", line
        print "FAILED"
        sys.exit(1)

# check if an event is legal;
#
def legal(e):
    global state, barber_awake, num_in_shop
    (type, cnum, action, line) = e

    if type == 'b' and action == 'wakes up':
        if barber_awake:
            return False
        if num_in_shop != 1:
            return False
    elif type == 'c' and action == 'enters shop':
        checkcustomer(cnum, line)
        if state[cnum] != 'O':
            return False
        if num_in_shop == 0 and barber_awake:
            return False
        if num_in_shop == 1 and not barber_awake:
            return False
        if num_in_shop > 5:
            return False
    elif type == 'c' and action == 'starts haircut':
        checkcustomer(cnum, line)
        if state[cnum] != 'L':
            return False
        if not barber_awake:
            return False
    elif type == 'c' and action == 'leaves shop':
        checkcustomer(cnum, line)
        if not state[cnum] in ('H', 'F'):
            return False
    elif type == 'b' and action == 'goes to sleep':
        if not barber_awake:
            return False
        if num_in_shop > 0:
            return False
    else:
        print "Internal error: event: ", e
        sys.exit(0)

    if verbose:
        print num_in_shop, line
    return True

waiting_id = 0

def perform(e):
    global state, barber_awake, num_in_shop, waiting_id
    (type, cnum, action, line) = e

    if type == 'c' and action == 'enters shop':
        num_in_shop += 1
        if num_in_shop > 5:
            state[cnum] = 'F'
            waiting_id = cnum
        else:
            state[cnum] = 'L'
    elif type == 'c' and action == 'starts haircut':
        state[cnum] = 'H'
    elif type == 'c' and action == 'leaves shop':
        num_in_shop -= 1
        if state[cnum] == 'H':
            pass
        if state[waiting_id] == 'F':
            state[waiting_id] = 'L'
        state[cnum] = 'O'
    elif type == 'b' and action == 'goes to sleep':
        barber_awake = False
    elif type == 'b' and action == 'wakes up':
        barber_awake = True
    else:
        print "Internal error: event: ", e
        sys.exit(0)


names = dict()
names['F'] = 'inside temporarily'
names['L'] = 'waiting in line'
names['H'] = 'getting haircut'
names['O'] = 'outside shop'

# for each time, take the set of events at that time and find one
# which is legal in the current state; update the state according to
# the event. Repeat until no events left for this timestamp, and go on
# to the next time interval. Fail if we find an indigestible event.
#

def evt_cmp(x,y):
    (action_x, action_y) = (x[2], y[2])
    if action_x == 'leaves shop' and action_y == 'enters shop':
        return 1
    if action_y == 'leaves shop' and action_x == 'enters shop':
        return -1
    return 0

status = True
for t in timeline:
    E = events[t]
    # E.sort(cmp=lambda x,y: order[x[2]]-order[y[2]])
    E.sort(cmp=evt_cmp)
    while E != []:
        e = None
        for _e in E:
            if legal(_e):
                e = _e
                perform(e)
                E.remove(e)
                break
        if e is None:
            status = False
            print "ERROR: Illegal lines:"
            for e in E:
                print "-->", e[3]
            print "State:"
            print "number in shop: %d" % num_in_shop
            for i in range(0,10):
                print "customer %d: %s" % (i, names[state[i]])
            print "barber: ", "awake" if barber_awake else "asleep"
            for e in events[t+1]:
                print "-->", e[3]
            break
    if not status:
        break

if status:
    print "SUCCESS"
else:
    print "FAILED"

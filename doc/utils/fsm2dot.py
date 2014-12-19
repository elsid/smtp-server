#!/usr/bin/env python3

import re
import sys

STATES_PATTERN = re.compile('state\s*=\s*([^,]+(?:\s*,\s*[^,]+)*)\s*;')
EVENTS_PATTERN = re.compile('event\s*=\s*([^,]+(?:\s*,\s*[^,]+)*)\s*;')
TRANSITION_PATTERN = re.compile(
    '{\s*'
        '(?:tst\s*=\s*"?(?P<source>[^";]+)"?\s*;\s*)'
        '(?:tev\s*=\s*"?(?P<event>[^";]+)"?\s*;\s*)'
        '(?:next\s*=\s*"?(?P<target>[^";]+)"?\s*;\s*)?'
    '}'
)
HEADER = '''
    rankdir = TB;
    ranksep = 1;
    nodesep = 0.05;
'''

def norm(s):
    return '"' + s.replace('_', ' ') + '"'

def print_header():
    print ('digraph {')
    print (HEADER)

def print_edge(src, edge, dst):
    print ('%s -> %s [label=%s];' % (norm(src), norm(dst), norm(edge)))

def print_footer():
    print ('}')

if '__main__' == __name__:
    states = {'init', 'done', 'invalid'}
    events = set()
    transitions = set()
    for line in sys.stdin:
        states_match = STATES_PATTERN.search(line)
        events_match = EVENTS_PATTERN.search(line)
        transitions_match = TRANSITION_PATTERN.search(line)
        if states_match:
            states |= set((x.strip() for x in states_match.group(1).split(',')))
        elif events_match:
            events |= set((x.strip() for x in events_match.group(1).split(',')))
        elif transitions_match:
            source = transitions_match.group('source')
            event = transitions_match.group('event')
            target = transitions_match.group('target')
            sources = set()
            transition_events = set()
            targets = set()
            if '*' == source:
                sources = states.difference({'done', 'invalid'})
            else:
                sources.add(source)
            if '*' == event:
                transition_events = events
            else:
                transition_events.add(event)
            if '*' == target:
                targets = states
            else:
                targets.add(target)
            for source in sources:
                for event in transition_events:
                    for target in targets:
                        if target is None:
                            transitions.add((source, event, source))
                        else:
                            transitions.add((source, event, target))
    print_header()
    for transition in transitions:
        print_edge(transition[0], transition[1], transition[2])
    print_footer()

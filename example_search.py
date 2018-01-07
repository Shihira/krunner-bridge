#!/usr/bin/env python

import os
import sys
import krunner_bridge
from krunner_bridge import datasource

dsrc = None

def update_datasource():
    global dsrc
    dsrc = \
        datasource.from_files("/home/shihira/Program/Scripts/*.py", "utilities-terminal", "Script") + \
        datasource.from_files("/home/shihira/Program/Scripts/*.sh", "utilities-terminal", "Script") + \
        datasource.from_files("/home/shihira/Program/Scripts/autostart/*.sh", "utilities-terminal", "Script") + \
        datasource.from_desktops("/usr/share/applications/*.desktop", "Application") + \
        datasource.from_desktops("/home/shihira/.local/share/applications/*.desktop", "Application")

    # eliminate duplications
    dsrc = map(lambda x: (x.text, x), dsrc)
    dsrc = list(dict(dsrc).values())

def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)

@krunner_bridge.init_handler
def init():
    pass

@krunner_bridge.match_handler
def match(query):
    global dsrc

    result = []
    rank = 1
    choice = 0

    update_datasource()

    # number character at the tail of query string is used for quick selection
    if query[-1] in list(map(str, range(1, 10))):
        choice = int(query[-1])
        query = query[:-1]

    for d in dsrc:
        if not d.fuzzy_match(query):
            continue
        d.text = "%d. %s" % (rank, d.text)
        d.relevance = 1 - 0.01 * rank

        result += [d]
        rank += 1

    # make exact match with quick selection
    if choice > 0 and choice <= len(result):
        result = [result[choice - 1]]
        result[0].relevance = 1

    return result[:9]

@krunner_bridge.run_handler
def run(data):
    import subprocess
    subprocess.Popen(["kioclient5", "exec", data])
    # clean up annoying history
    subprocess.Popen(["sed", "-i", "s/^history.*/history=/g",
            os.path.expanduser("~/.config/krunnerrc")])

if __name__ == "__main__":
    krunner_bridge.exec()


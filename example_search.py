#!/usr/bin/env python

import os
import sys
import time
import pickle
import krunner_bridge
from krunner_bridge import datasource

def update_datasource():
    dsrc = \
        datasource.from_files("/home/shihira/Program/Scripts/*.py", "utilities-terminal", "Script") + \
        datasource.from_files("/home/shihira/Program/Scripts/*.sh", "utilities-terminal", "Script") + \
        datasource.from_files("/home/shihira/Program/Scripts/autostart/*.sh", "utilities-terminal", "Script") + \
        datasource.from_desktops("/usr/share/applications/*.desktop", "Application") + \
        datasource.from_desktops("/home/shihira/.local/share/applications/*.desktop", "Application")

    # eliminate duplications
    dsrc = map(lambda x: (x.text, x), dsrc)
    dsrc = list(dict(dsrc).values())

    for i in dsrc:
        if not i.text.endswith(".py") and not i.text.endswith(".sh"):
            continue
        i.text = i.text.replace("_", " ").replace(".py", "").replace(".sh", "").title()

    return dsrc

def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)

@krunner_bridge.init_handler
def init():
    dsrc = update_datasource()
    last_update = time.time()
    with open("/tmp/krunner_bridge_search_cache", "wb") as f:
        pickle.dump((last_update, dsrc), f)

@krunner_bridge.match_handler
def match(query):
    with open("/tmp/krunner_bridge_search_cache", "rb") as f:
        last_update, dsrc = pickle.load(f)
    if last_update < time.time() - 30 and os.fork() == 0:
        # fork a new process to update cache
        init()
        exit()

    result = []
    rank = 1
    choice = 0

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


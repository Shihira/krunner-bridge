#!/usr/bin/env python

import os
import time
import copy
import threading

import krunner_bridge
from krunner_bridge import datasource

class desktop_search:
    last_update = 0
    dsrc = []

    def _collect_datasource(self):
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

    def init(self):
        if self.last_update < time.time() - 30:
            self.dsrc = self._collect_datasource()
            self.last_update = time.time()

    def match(self, query):
        result = []
        rank = 1
        choice = 0

        # number character at the tail of query string is used for quick selection
        if query[-1] in list(map(str, range(1, 10))):
            choice = int(query[-1])
            query = query[:-1]

        for d in self.dsrc:
            if not d.fuzzy_match(query):
                continue

            entry = copy.deepcopy(d)
            entry.text = "%d. %s" % (rank, entry.text)
            entry.relevance = 1 - 0.01 * rank

            result += [entry]
            rank += 1

        # make exact match with quick selection
        if choice > 0 and choice <= len(result):
            result = [result[choice - 1]]
            result[0].relevance = 1

        return result[:9]

    def run(self, data):
        import subprocess
        subprocess.Popen(["kioclient5", "exec", data])
        # clean up annoying history
        subprocess.Popen(["sed", "-i", "s/^history.*/history=/g",
                os.path.expanduser("~/.config/krunnerrc")])

if __name__ == "__main__":
    searcher = desktop_search()

    krunner_bridge.init_handler(lambda: searcher.init())
    krunner_bridge.match_handler(lambda query: searcher.match(query))
    krunner_bridge.run_handler(lambda data: searcher.run(data))

    krunner_bridge.exec()


#!/usr/bin/env python

import krunner_bridge
from math import *

@krunner_bridge.match_handler
def match(query):
    try:
        res = eval(query)
        if isinstance(res, (int, float)):
            return krunner_bridge.datasource(
                text=str(res),
                icon="accessories-calculator",
                category="Python Calculator")
    except:
        pass

if __name__ == "__main__":
    krunner_bridge.exec()

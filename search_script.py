#!/usr/bin/env python

import os
import re
import sys
import json
import glob

from PyQt5 import QtCore

def __match(q, f):
    pattern = r"(\s|^)" + r"(.*\s|)".join(q)
    splitted_fn = re.sub(r"(/|_|-|\.|\(|\))", r" \1 ", f)
    splitted_fn = re.sub(r"([A-Z])", r" \1", splitted_fn)

    return bool(re.search(pattern, splitted_fn, re.IGNORECASE))

def __datasource_words_files(glob_pattern, icon, cat=""):
    ds = __datasource_files(glob_pattern, icon, cat)
    for d in ds:
        t = d["text"]
        t = os.path.splitext(t)[0]
        t = t.split("_")
        t = map(lambda x: x.title(), t)
        t = " ".join(t)
        d["text"] = t
    return ds

def __datasource_files(glob_pattern, icon, cat=""):
    ds = glob.iglob(glob_pattern, recursive=True)

    ds = map(lambda x: {
        "data": x,
        "text": os.path.basename(x),
        "cmp": cat + " " + os.path.basename(x),
        "icon": icon,
        "category": cat,
    }, ds)

    return list(ds)

def __datasource_desktops(glob_pattern, cat=""):
    ds = glob.iglob(glob_pattern, recursive=True)

    def dsk_map(x):
        dsk = QtCore.QSettings(x, QtCore.QSettings.IniFormat)
        dsk.beginGroup("Desktop Entry")
        return {
            "data": x,
            "text": dsk.value("Name"),
            "icon": dsk.value("Icon"),
            "cmp": cat + " " + dsk.value("Name"),
            "category": cat,
        }

    ds = map(dsk_map, ds)

    return list(ds)

__DS = \
    __datasource_words_files("/home/shihira/Program/Scripts/*.py", "utilities-terminal", "Script") + \
    __datasource_words_files("/home/shihira/Program/Scripts/*.sh", "utilities-terminal", "Script") + \
    __datasource_words_files("/home/shihira/Program/Scripts/autostart/*.sh", "utilities-terminal", "Script") + \
    __datasource_desktops("/usr/share/applications/*.desktop", "Application") + \
    __datasource_desktops("/home/shihira/.local/share/applications/*.desktop", "Application")

__DS = map(lambda x: (x["text"], x), __DS)
__DS = list(dict(__DS).values())

def op_match(query):
    if not query: return

    result = []
    rank = 1
    choise = 0

    if query[-1] in list(map(str, range(1, 10))):
        choise = int(query[-1])
        query = query[:-1]

    for d in __DS:
        if not __match(query, d["cmp"]):
            continue
        result += [{
            "text": str(rank) + ". " + d["text"],
            "relevance": 1 - 0.01 * rank,
            "data": d["data"],
            "iconName": d["icon"],
            "matchCategory": d["category"],
        }]

        rank += 1

    if choise > 0 and choise <= len(result):
        result = [result[choise - 1]]
        result[0]["relevance"] = 1

    print(json.dumps({ "result": result[:9] }))

def op_init():
    pass

def op_run(data):
    import subprocess
    subprocess.Popen(["kioclient5", "exec", data])
    subprocess.Popen(["sed", "-i", "s/^history.*/history=/g", "/home/shihira/.config/krunnerrc"])

if __name__ == "__main__":
    args = sys.stdin.read()
    args = json.loads(args)

    op = args["operation"]
    del args["operation"]
    globals()["op_" + op](**args)



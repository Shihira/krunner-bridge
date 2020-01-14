import re
import json
import os
import sys
import glob
import configparser

def split_into_words(s):
    sf = re.sub(r"(/|_|-|\.|\(|\))", r" \1 ", s)
    sf = re.sub(r"([A-Z])", r" \1", sf)
    return sf

class datasource(object):
    query_cache = ["", None]

    def __init__(self, **kwargs):
        self.data = kwargs.get("data") # save a dict/array/string/numeric here for later use
        self.text = kwargs.get("text") # display text
        self.cmp = kwargs.get("cmp") # string for matcher comparison
        self.icon = kwargs.get("icon") # icon name. check them in `kdialog --geticon`
        self.category = kwargs.get("category") # display category text
        self.relevance = kwargs.get("relevance", 0)

    @staticmethod
    def from_files(glob_pattern, icon, cat=""):
        ds = glob.iglob(glob_pattern, recursive=True)

        ds = map(lambda x: datasource(
            data = x,
            text = os.path.basename(x),
            cmp = split_into_words(cat + " " + os.path.basename(x)),
            icon = icon,
            category = cat), ds)

        return list(ds)

    @staticmethod
    def from_desktops(glob_pattern, cat=""):
        ds = glob.iglob(glob_pattern, recursive=True)

        def dsk_map(x):
            try:
                dsk = configparser.ConfigParser()
                dsk.read(x)
                de = dsk["Desktop Entry"]

                d = datasource(
                    data = x,
                    text = de.get("Name"),
                    icon = de.get("Icon"),
                    cmp = split_into_words(cat + " " + de.get("Name")),
                    category = cat)

                return d
            except:
                pass

        ds = filter(bool, map(dsk_map, ds))

        return list(ds)

    def fuzzy_match(self, q):
        '''
        "ks"   matches "kde-system_settings"
        "kss"  matches "KDE System Settings"
        "kss"  matches "kde/system/settings(old).cpp"
        "ksc"  matches "kde/system/settings(old).cpp"
        "ksys" matches "KDE System Settings"
        "kst"  doesn't match "KDE System Settings"
        '''

        if self.query_cache[0] != q or not self.query_cache[0]:
            pattern = list(map(lambda x: "\\u%04x" % ord(x), q))
            pattern = r"(\s|^)" + r"(.*\s|)".join(pattern)

            self.query_cache[0] = q
            self.query_cache[1] = re.compile(pattern, re.IGNORECASE)

        return bool(self.query_cache[1].search(self.cmp))

    def __repr__(self):
        return "<krunner_bridge.datasource text={0} cmp={1}>".format(
            repr(self.text), repr(self.cmp))

__MATCH_HANDLER = lambda query: []
def match_handler(func):
    global __MATCH_HANDLER
    __MATCH_HANDLER = func
    return func

__INIT_HANDLER = lambda: None
def init_handler(func):
    global __INIT_HANDLER
    __INIT_HANDLER = func
    return func

__RUN_HANDLER = lambda data: None
def run_handler(func):
    global __RUN_HANDLER
    __RUN_HANDLER = func
    return func

def exec():
    args = sys.stdin.read()
    args = json.loads(args)

    op = args["operation"]
    del args["operation"]

    if op == "match":
        ds = __MATCH_HANDLER(**args)

        if isinstance(ds, datasource):
            ds = [ds]
        if not ds: ds = []

        result = list(map(lambda x: {
            "text": x.text,
            "relevance": x.relevance,
            "data": x.data,
            "iconName": x.icon,
            "matchCategory": x.category,
        }, ds))

        print(json.dumps({ "result": result }))

    if op == "init":
        __INIT_HANDLER(**args)

    if op == "run":
        __RUN_HANDLER(**args)


#!/usr/bin/env python3

import krunner_bridge

# Make sure that you add this script to the config file at ~/.local/share/kservices5/krunner_bridge.desktop
# It is recommended to use the config group system, this allows you to enable/disable optional methods like setup/teardown

@krunner_bridge.init_handler
def init():
    pass

@krunner_bridge.match_handler
def match(query):
    return krunner_bridge.datasource(
        text="Hello There!",
        icon="planetkde",
        category="Python Calculator")

@krunner_bridge.run_handler
def run(data):
    pass

@krunner_bridge.setup_handler
def setup():
    pass

@krunner_bridge.teardown_handler
def teardown():
    pass


if __name__ == "__main__":
    krunner_bridge.exec()

#!/bin/bash

if [ ! -f example.py ]; then
    echo "example.py is missing. Aborted."
fi

if [ ! -d build ]; then
    mkdir build
fi

cd build
cmake ..
make
make install

install ../example_search.py $HOME/.local/share/kservices5
install ../example_calc.py $HOME/.local/share/kservices5
install ../krunner_bridge.py $HOME/.local/share/kservices5

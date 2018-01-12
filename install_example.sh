#!/bin/bash

if [ ! -f example_search.py ]; then
    echo "example.py is missing. Aborted."
    exit
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

echo X-KRunner-Bridge-Script-1=example_search.py >> $HOME/.local/share/kservices5/krunner_bridge.desktop
echo X-KRunner-Bridge-Script-2=example_calc.py >> $HOME/.local/share/kservices5/krunner_bridge.desktop

#!/bin/bash

if [[ ! -f example_search.py || ! -f example_calc.py || ! -f krunner_bridge.py ]]; then
    echo "Project files are missing. Aborted."
    exit
fi

if [[ ! -d build ]]; then
    mkdir build
fi

cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
make install

install ../example_search.py $HOME/.local/share/kservices5
install ../example_calc.py $HOME/.local/share/kservices5
install ../krunner_bridge.py $HOME/.local/share/kservices5

echo X-KRunner-Bridge-Script-1=example_search.py >> $HOME/.local/share/kservices5/krunner_bridge.desktop
echo X-KRunner-Bridge-Script-2=example_calc.py >> $HOME/.local/share/kservices5/krunner_bridge.desktop

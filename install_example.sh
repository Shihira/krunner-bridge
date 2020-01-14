#!/bin/bash

cd python
python3 setup.py install --user
cd ..

if [[ ! -d build ]]; then
    mkdir build
fi

cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
make install

install ../example_search.py $HOME/.local/share/kservices5
install ../example_calc.py $HOME/.local/share/kservices5

echo X-KRunner-Bridge-Script-1=example_search.py >> $HOME/.local/share/kservices5/krunner_bridge.desktop
echo X-KRunner-Bridge-Script-2=example_calc.py >> $HOME/.local/share/kservices5/krunner_bridge.desktop

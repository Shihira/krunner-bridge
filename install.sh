#!/bin/bash

# Exit immediately if something fails
set -e

cd python
python3 setup.py install --user
cd ..

if [[ ! -d build ]]; then
    mkdir build
fi

cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j $(nproc)
make install

cd ..
install ./example_search.py $HOME/.local/share/kservices5
install ./example_calc.py $HOME/.local/share/kservices5
install ./debug_helper.sh $HOME/.local/share/kservices5

kquitapp5 krunner 2> /dev/null
kstart5 --windowclass krunner krunner > /dev/null 2>&1 &

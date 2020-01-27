#!/bin/bash

# Just send the data as a notification
notify-send $1

# Display some hello world text inside of KRunner
echo '{"result":[{"text":"Hello World", "iconName":"planetkde"}]}'

#!/bin/bash 
# MindBx startup script

command=$1 

if [ $command == "start" ]; then
    atlants-DATA_interface/data-daemon/data_interface atlants-DATA_interface/data-daemon/config/data_config_player1.xml &
    atlants-DATA_interface/data-daemon/data_interface atlants-DATA_interface/data-daemon/config/data_config_player2.xml &
	atlants-DATA_preprocessing/preprocess-daemon/data_preprocessing atlants-DATA_preprocessing/preprocess-daemon/config/preprocess_config_player1.xml &
	atlants-DATA_preprocessing/preprocess-daemon/data_preprocessing atlants-DATA_preprocessing/preprocess-daemon/config/preprocess_config_player2.xml &
	atlants-cerebral_wars/cerebral_wars_app atlants-cerebral_wars/config/cerebral_wars_config.xml &
else
    killall cerebral_wars_app
    killall data_interface
    killall data_preprocessing
fi

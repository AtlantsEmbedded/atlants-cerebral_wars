#!/bin/sh /etc/rc.common
# MindBx startup script

START=1
STOP=2

start() {

	hciconfig hci0 sspmode 1

	/intelli/data/data_interface /intelli/data/config/data_config_player1.xml &
	/intelli/data/data_interface /intelli/data/config/data_config_player2.xml &
	sleep 2
	/intelli/data/data_preprocessing /intelli/data/config/preprocess_config_player1.xml &
	/intelli/data/data_preprocessing /intelli/data/config/preprocess_config_player2.xml &
	sleep 2
	/intelli/app/cerebral_wars /intelli/app/config/cerebral_wars_config.xml &
}

stop() {
        killall data_interface
        killall data_preprocessing
        killall cerebral_wars
}

boot() {
	hciconfig hci0 sspmode 1

	/intelli/data/data_interface /intelli/data/config/data_config_player1.xml &
	/intelli/data/data_interface /intelli/data/config/data_config_player2.xml &
	sleep 2
	/intelli/data/data_preprocessing /intelli/data/config/preprocess_config_player1.xml &
	/intelli/data/data_preprocessing /intelli/data/config/preprocess_config_player2.xml &
	sleep 2
	/intelli/app/cerebral_wars /intelli/app/config/cerebral_wars_config.xml &

}

#!/bin/bash
x_pos=($( shuf -i 75-1500 -n 9))
y_pos=($( shuf -i 75-900 -n 9))
xterm -geometry 150×32+${x_pos[1]}+${y_pos[1]} -e bash -c './key_value_store_server ../../config.txt 1' &
xterm -geometry 150×32+${x_pos[2]}+${y_pos[2]} -e bash -c './key_value_store_server ../../config.txt 2' &
xterm -geometry 150×32+${x_pos[3]}+${y_pos[3]} -e bash -c './key_value_store_server ../../config.txt 3' &
xterm -geometry 150×32+${x_pos[4]}+${y_pos[4]} -e bash -c './key_value_store_server ../../config.txt 4' &
xterm -geometry 150×32+${x_pos[2]}+${y_pos[2]} -e bash -c './key_value_store_server ../../config.txt 5' &
xterm -geometry 150×32+${x_pos[3]}+${y_pos[3]} -e bash -c './key_value_store_server ../../config.txt 6' &
xterm -geometry 150×32+${x_pos[4]}+${y_pos[4]} -e bash -c './key_value_store_server ../../config.txt 7' &
xterm -geometry 150×32+${x_pos[5]}+${y_pos[5]} -e bash -c './key_value_store_master ../../config.txt' &
xterm -geometry 150×32+${x_pos[6]}+${y_pos[6]} -e bash -c './frontend_master ../../config_front.txt 0' &
sleep 0.5
xterm -geometry 150×32+${x_pos[7]}+${y_pos[7]} -e bash -c './webserver ../../config_front.txt 1' &
xterm -geometry 150×32+${x_pos[8]}+${y_pos[8]} -e bash -c './webserver ../../config_front.txt 2' &
xterm -geometry 150×32+${x_pos[9]}+${y_pos[9]} -e bash -c './webserver ../../config_front.txt 3' &
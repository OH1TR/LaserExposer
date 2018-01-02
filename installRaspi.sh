#!/bin/sh
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install python3-pip 
sudo pip3 install Pillow
sudo apt-get install libopenjp2-7 libtiff5
sudo pip3 install numpy
sudo apt-get install libblas-dev liblapack-dev
sudo pip3 install pyserial


#!/bin/bash

sudo apt -y update

sudo snap install cmake --classic
sudo apt -y install make
sudo apt -y install clang


for (( i=8000; i<=8500; ++i))
do
        ufw allow $i
done

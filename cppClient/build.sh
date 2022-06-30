#! /bin/bash

source /etc/profile  # 激活高版本g++
if [ -a build ];then
    rm -rf build
fi
mkdir build
cd build
cmake ../ 
make
cd ..
echo "请开始你的表演"
if [ -x build/client ];then
    build/client localhost 9999
fi

#! /bin/bash

source /etc/profile  # 激活高版本g++
if [ -a build ];then
    rm -rf build
fi
mkdir build
cd build
cmake ../server
make
cd ..
echo "ok server准备启动"
if [ -x build/server ];then
    build/server 9999
fi

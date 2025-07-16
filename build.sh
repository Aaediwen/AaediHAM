#!/bin/bash

mkdir build 
cd build
cp -rv ../images .
cp -rv ../*.ttf .
cmake ..
cmake --build .

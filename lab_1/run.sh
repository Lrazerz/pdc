#!/bin/bash

rm -rf main_1 main_2 main_3 > /dev/null

echo 'Pure:'
g++ main.cpp -o main_1 -O2 -Wall -pedantic -std=c++17 > /dev/null
./main_1 example

echo 'MMX:'
g++ main.cpp -o main_2 -O2 -Wall -pedantic -DMMX -std=c++17 > /dev/null
./main_2 example


echo 'SSE:'
g++ main.cpp -o main_3 -O2 -Wall -pedantic -DSSE -std=c++17 > /dev/null
./main_3 example_float

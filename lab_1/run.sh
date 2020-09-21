#!/bin/bash

rm -rf main_1 main_2> /dev/null

echo 'Without MMX:'
g++ main.cpp -o main_1 -O0 -Wall -pedantic -g3 > /dev/null
./main_1 example

echo 'With MMX:'
g++ main.cpp -o main_2 -O0 -Wall -pedantic -DMMX -g3 > /dev/null
./main_2 example

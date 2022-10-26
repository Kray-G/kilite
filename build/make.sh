#!/bin/sh

CUR=$PWD
cd `basename $0`
cd ..

mkdir -p bin/mir
cd bin/mir
cmake ../../mir
make
cp -f c2m ../
cp -f libmir_static.a ../

cd ..
gcc -O2 -o kilite \
    ../src/main.c \
    ../src/frontend/lexer.c \
    ../src/frontend/parser.c \
    ../src/frontend/error.c \
    ../src/frontend/dispast.c \
    ../src/frontend/mkkir.c \
    ../src/backend/dispkir.c \
    ../src/backend/translate.c

cd $CUR

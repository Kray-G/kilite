#!/bin/sh

CUR=$PWD
cd `dirname $0`
echo $PWD
cd ..

mkdir -p bin/mir
cd bin/mir
cmake ../../mir
make
cp -f c2m ../
cp -f libmir_static.a ../
cd ..

TEMPF=h.c

# Create a header source code.
gcc -O2 -o makecstr ../build/makecstr.c
cat ../src/template/lib/bign.h > $TEMPF
cat ../src/template/lib/bigz.h >> $TEMPF
cat ../src/template/lib/printf.h >> $TEMPF
cat ../src/template/header.h >> $TEMPF
./makecstr $TEMPF > ../src/backend/header.c
rm $TEMPF

gcc -Wno-unused-result -O2 -I ../mir \
    -DUSE_INT64 -o kilite \
    ../src/main.c \
    ../src/frontend/lexer.c \
    ../src/frontend/parser.c \
    ../src/frontend/error.c \
    ../src/frontend/dispast.c \
    ../src/frontend/mkkir.c \
    ../src/frontend/opt/type.c \
    ../src/backend/dispkir.c \
    ../src/backend/translate.c \
    ../src/backend/resolver.c \
    ../src/backend/header.c \
    ../src/backend/cexec.c \
    ../bin/libmir_static.a

cp -f kilite ../kilite

mkdir -p lib
cp -f ../src/template/lib/bign.h ./lib/bign.h
cp -f ../src/template/lib/bigz.h ./lib/bigz.h
cp -f ../src/template/lib/printf.h ./lib/printf.h
cp -f ../src/template/common.h ./common.h
cp -f ../src/template/header.h ./header.h
cat ../src/template/lib/bign.c > $TEMPF
cat ../src/template/lib/bigz.c >> $TEMPF
cat ../src/template/lib/printf.c >> $TEMPF
cat ../src/template/alloc.c >> $TEMPF
cat ../src/template/gc.c >> $TEMPF
cat ../src/template/init.c >> $TEMPF
cat ../src/template/main.c >> $TEMPF
cat ../src/template/util.c >> $TEMPF
cat ../src/template/format.c >> $TEMPF
cat ../src/template/bigi.c >> $TEMPF
cat ../src/template/str.c >> $TEMPF
cat ../src/template/obj.c >> $TEMPF
cat ../src/template/op.c >> $TEMPF
cat ../src/template/libstd.c >> $TEMPF
./c2m -DUSE_INT64 -I lib -c $TEMPF
rm kilite.bmir
mv h.bmir kilite.bmir
cp -f kilite.bmir ../kilite.bmir
rm $TEMPF

rm -f *.h
rm -f makecstr
rm -rf lib

cd $CUR

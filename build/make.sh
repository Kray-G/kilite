#!/bin/bash

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

BIN=$PWD
TEMPF=$BIN/libkilite.c

# Create a header source code.
echo Generating a header source code...
gcc -O3 -o makecstr ../build/makecstr.c
gcc -O2 -o clockspersec ../build/clockspersec.c
cat ../src/template/lib/bign.h > $TEMPF
cat ../src/template/lib/bigz.h >> $TEMPF
cat ../src/template/lib/printf.h >> $TEMPF
cat ../src/template/header.h >> $TEMPF
./makecstr $TEMPF > ../src/backend/header.c
rm $TEMPF

echo Building a Kilite binary...
gcc -Wno-unused-result -O3 -I ../mir \
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

echo Creating a basic library...
echo "#define KILITE_AMALGAMATION" > $TEMPF
./clockspersec >> $TEMPF
cat ../src/template/lib/bign.h >> $TEMPF
cat ../src/template/lib/bigz.h >> $TEMPF
echo "#ifdef __MIRC__" >> %TEMPF%
cat ../src/template/lib/printf.h >> $TEMPF
echo "#endif" >> %TEMPF%
cat ../src/template/header.h >> $TEMPF
cat ../src/template/lib/bign.c >> $TEMPF
cat ../src/template/lib/bigz.c >> $TEMPF
echo "#ifdef __MIRC__" >> %TEMPF%
cat ../src/template/lib/printf.c >> $TEMPF
echo "#endif" >> %TEMPF%
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
cat ../src/template/inc/platform.c >> $TEMPF
cd ../src/template/std
$BIN/kilite --makelib callbacks.klt >> $TEMPF
cd $BIN

echo Generating a static library file for gcc...
gcc -O3 -DUSE_INT64 -o ${TEMPF/.c/.o} -I lib -Wno-unused-result -c $TEMPF
ar rcs libkilite.a ${TEMPF/.c/.o}
cp -f libkilite.a ../libkilite.a
./c2m -DUSE_INT64 -I lib -c $TEMPF
rm -f kilite.bmir
mv libkilite.bmir kilite.bmir
cp -f kilite.bmir ../kilite.bmir

# rm $TEMPF
rm -f *.h
rm -f makecstr
echo Building Kilite modules successfully ended.

cd $CUR

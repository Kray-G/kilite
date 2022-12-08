#!/bin/bash

CUR=$PWD
cd `dirname $0`
echo $PWD
cd ..

echo Building MIR...
mkdir -p bin/mir
cd bin/mir
cmake ../../submodules/mir
make
cp -f c2m ../
cp -f libmir_static.a ../
cd ..

echo Building zlib...
mkdir -p zlib
cd zlib
cmake -DCMAKE_INSTALL_PREFIX=../../src/template/inc/lib/zlib ../../submodules/zlib
make
make install
cp -f libz.a ../
cd ..

echo Building minizip...
mkdir -p minizip
cd minizip
cmake -DMZ_BZIP2=OFF -DMZ_LZMA=OFF -DMZ_ZSTD=OFF -DZLIB_ROOT=../../src/template/inc/lib/zlib -DCMAKE_INSTALL_PREFIX=../../src/template/inc/lib/minizip ../../submodules/minizip-ng
make
make install
cp -f libminizip.a ../
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
gcc -Wno-unused-result -O3 -I../submodules/mir \
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
    -L../bin \
    -lmir_static \
    -lminizip \
    -lz

cp -f kilite ../kilite

echo Creating a basic library...
echo "#define KILITE_AMALGAMATION" > $TEMPF
./clockspersec >> $TEMPF
cat ../src/template/lib/bign.h >> $TEMPF
cat ../src/template/lib/bigz.h >> $TEMPF
echo "#ifdef __MIRC__" >> $TEMPF
cat ../src/template/lib/printf.h >> $TEMPF
echo "#endif" >> $TEMPF
cat ../src/template/header.h >> $TEMPF
cat ../src/template/lib/bign.c >> $TEMPF
cat ../src/template/lib/bigz.c >> $TEMPF
echo "#ifdef __MIRC__" >> $TEMPF
cat ../src/template/lib/printf.c >> $TEMPF
echo "#endif" >> $TEMPF
echo "#line 1 \"alloc.c\"" >> $TEMPF
cat ../src/template/alloc.c >> $TEMPF
echo "#line 1 \"gc.c\"" >> $TEMPF
cat ../src/template/gc.c >> $TEMPF
echo "#line 1 \"init.c\"" >> $TEMPF
cat ../src/template/init.c >> $TEMPF
echo "#line 1 \"main.c\"" >> $TEMPF
cat ../src/template/main.c >> $TEMPF
echo "#line 1 \"util.c\"" >> $TEMPF
cat ../src/template/util.c >> $TEMPF
echo "#line 1 \"format.c\"" >> $TEMPF
cat ../src/template/format.c >> $TEMPF
echo "#line 1 \"bigi.c\"" >> $TEMPF
cat ../src/template/bigi.c >> $TEMPF
echo "#line 1 \"str.c\"" >> $TEMPF
cat ../src/template/str.c >> $TEMPF
echo "#line 1 \"bin.c\"" >> $TEMPF
cat ../src/template/bin.c >> $TEMPF
echo "#line 1 \"obj.c\"" >> $TEMPF
cat ../src/template/obj.c >> $TEMPF
echo "#line 1 \"op.c\"" >> $TEMPF
cat ../src/template/op.c >> $TEMPF
echo "#line 1 \"lib.h\"" >> $TEMPF
cat ../src/template/lib.h >> $TEMPF
echo "#line 1 \"libstd.c\"" >> $TEMPF
cat ../src/template/libstd.c >> $TEMPF
echo "#line 1 \"libxml.c\"" >> $TEMPF
cat ../src/template/libxml.c >> $TEMPF
echo "#line 1 \"libzip.c\"" >> $TEMPF
cat ../src/template/libzip.c >> $TEMPF
echo "#line 1 \"inc/platform.c\"" >> $TEMPF
cat ../src/template/inc/platform.c >> $TEMPF
cd ../src/template/std
$BIN/kilite --makelib callbacks.klt >> $TEMPF
cd $BIN

echo Generating a static library file for gcc...
gcc -O3 -DUSE_INT64 -o ${TEMPF/.c/.o} -Ilib -I../src/template -I../src/template/inc -Wno-unused-result -c $TEMPF
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

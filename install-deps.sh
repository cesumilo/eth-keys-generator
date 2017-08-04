# Author: Guillaume ROBIN <robinguillaume.pro@gmail.com>
# Date: 04/08/2017

function check_error() {
    if [ "$1" != "0" ]; then
        (>&2 echo -e "error($1): $2")
        exit 1
    fi
}

echo "[*] Cleaning previous installations."
if [ -d "./lib/include" ]; then
    rm -r ./lib/include
fi

if [ -d "./lib/bin" ]; then
    rm -r ./lib/bin
fi

mkdir -p lib/include
mkdir -p lib/bin

echo "[*] Installing argparser."
cd lib/argparser
make c
check_error $? "something went wrong during argparser compilation."
cp bin/argparser.so ../bin/libargparser.so

echo "[*] Installing libkeccak."
cd ../libkeccak
make
check_error $? "something went wrong during libkeccak compilation."
cp bin/lib* ../bin/

echo "[*] Installation sha3sum."
cd ../sha3sum
CFLAGS="-isystem $PWD/../include" LB_LIBRARY_PATH=$PWD/../ make
check_error $? "something went wrong during sha3sum compilation."
export PATH=$PATH:$PWD/bin

echo "[OK] All done."

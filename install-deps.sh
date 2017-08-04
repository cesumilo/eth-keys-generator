# Author: Guillaume ROBIN <robinguillaume.pro@gmail.com>
# Date: 04/08/2017

function check_error() {
    if [ "$1" != "0" ]; then
        (>&2 echo -e "error($1): $2")
        exit 1
    fi
}

function check_os() {
    gawk -F= '/^NAME/{print $2}' /etc/os-release | grep "Ubuntu"
    if [ "$?" != "0" ]; then
        (>&2 echo "[WARNING] This script has been tested only on Ubuntu.\nPress ENTER to continue or CTRL-C to exit.")
        read
    fi
}

function check_deps() {
    dpkg-query -W -f='${Status}\n' texinfo | grep "^install ok"
    check_error $? "texinfo package not installed.\nYou can use the following command line to solve this issue:\n\tsudo apt-get install texinfo"

    dpkg-query -W -f='${Status}\n' openssl | grep "^install ok"
    check_error $? "openssl package not installed.\nYou can use the following command line to solve this issue:\n\tsudo apt-get install openssl"

    dpkg-query -W -f='${Status}\n' make | grep "^install ok"
    check_error $? "openssl package not installed.\nYou can use the following command line to solve this issue:\n\tsudo apt-get install make"
}

function clean_prev_installs() {
    if [ -d "./lib/include" ]; then
        rm -r ./lib/include
    fi

    if [ -d "./lib/bin" ]; then
        rm -r ./lib/bin
    fi

    mkdir -p lib/include
    mkdir -p lib/bin
}

function install_deps() {
    echo "[*] Installing auto-auto-complete."
    cd lib/auto-auto-complete
    make
    check_error $? "something went wrong during auto-auto-complete compilation."
    export PATH=$PATH:$PWD/bin

    echo "[*] Installing argparser."
    cd ../argparser
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
    SHA3_SUM_PATH=$PATH:$PWD/bin
}

if [ -n "$1" ] && [ "$1" = "clean" ]; then
    cd lib/auto-auto-complete
    make clean
    cd ../argparser
    make clean
    cd ../libkeccak
    make clean
    cd ../sha3sum
    make clean 
    exit 0
fi

echo "[*] Checking operating system."
check_os

echo "[*] Checking for dependencies."
check_deps

echo "[*] Cleaning previous installations."
clean_prev_installs

echo "[*] Installing dependencies."
install_deps

echo -e "[OK] All done.\nUse the following command line to complete the installation:\n\texport PATH=$SHA3_SUM_PATH"

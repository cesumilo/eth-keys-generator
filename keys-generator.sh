# Name: keys-generator
# Author: Guillaume ROBIN <robinguillaume.pro@gmail.com>
# Date: 04/08/2017

if [ -z "$1" ]; then
    (>&2 echo -e "error: no name provided.\nUsage:\n\r./keys-generator name")
    exit 1
fi

function parse_args() {
    create_account=0
    if [ "$1" = "--create-account" ] || [ "$1" = "-c" ]; then
        create_account=1
    else
        key_name=$1
    fi

    if [ -n "$2" ] && [ "$2" = "-c" ]; then
        create_account=1
    elif [ -n "$2" ] && [ "$2" = "--create-account" ]; then
        create_account=1
    elif [ -n "$2" ]; then
        key_name=$2
    fi
}

parse_args $1 $2

export LD_LIBRARY_PATH=$PWD/lib/bin

echo -e "----------------- Keys Generator -----------------"
echo -e "-      Algorithm (elliptic curve): secp256k1     -"
echo -e "--------------------------------------------------"

sudo openssl ecparam -name secp256k1 -genkey -noout | openssl ec -text -noout > key.tmp
cat key.tmp |grep pub -A 5 | tail -n +2 | tr -d '\n[:space:]:' | sed 's/^04//' > ${key_name}.pub
cat ${key_name}.pub | keccak-256sum -x -l | tr -d ' -' | tail -c 41 > ${key_name}_addr.txt
cat key.tmp |grep priv -A 3 | tail -n +2 | tr -d '\n[:space:]:' | sed 's/^00//' > ${key_name}.key
rm key.tmp
echo -e "Ethereum address: `cat ${key_name}_addr.txt`"

if [ "$create_account" = "1" ]; then
    geth account import ${key_name}.key | grep "`cat ${key_name}_addr.txt`"
    if [ "$?" = 0 ]; then
        echo "Your account has been created!"
    else
        (>&2 echo "Cannot create account with geth and private key.")
        exit 1
    fi
fi

echo "All done."
exit 0

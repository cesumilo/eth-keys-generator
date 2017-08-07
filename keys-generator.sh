# Name: keys-generator
# Author: Guillaume ROBIN <robinguillaume.pro@gmail.com>
# Date: 04/08/2017

if [ -z "$1" ]; then
    (>&2 echo -e "error: no name provided.\nUsage:\n\r./keys-generator name [-c or --create-account] [--keygen]")
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

    create_node_keypair=0
    if [ -n "$3" ] && [ "$3" = "--keygen" ]; then
        create_node_keypair=1
    fi
}

parse_args $1 $2 $3

export LD_LIBRARY_PATH=$PWD/lib/bin

echo -e "----------------- Keys Generator -----------------"
echo -e "-      Algorithm (elliptic curve): secp256k1     -"
echo -e "--------------------------------------------------"

sudo openssl ecparam -name secp256k1 -genkey -noout | openssl ec -text -noout > key.tmp
cat key.tmp |grep pub -A 5 | tail -n +2 | tr -d '\n[:space:]:' | sed 's/^04//' > ${key_name}_acc.pub
cat ${key_name}_acc.pub | keccak-256sum -x -l | tr -d ' -' | tail -c 41 > ${key_name}_addr.txt
cat key.tmp |grep priv -A 3 | tail -n +2 | tr -d '\n[:space:]:' | sed 's/^00//' > ${key_name}_acc.key
rm key.tmp
account_addr=`cat ${key_name}_addr.txt`
echo -e "Ethereum address: ${account_addr}"

if [ "$create_account" = "1" ]; then
    echo -n Password: 
    read -s password
    echo
    echo $password > pass.tmp

    geth --password pass.tmp account import ${key_name}.key | grep "$account_addr"
    if [ "$?" = 0 ]; then
        rm pass.tmp
        echo "Your account has been created!"
        cat $HOME/.ethereum/keystore/`ls $HOME/.ethereum/keystore | grep "$account_addr"` > ${key_name}key
    else
        rm pass.tmp
        (>&2 echo "Cannot create account with geth and private key.")
        exit 1
    fi
fi

if [ "$create_node_keypair" = "1" ]; then
    constellation-node --generatekeys=${key_name}
fi

echo "All done."
exit 0

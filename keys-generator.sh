# Name: keys-generator
# Author: Guillaume ROBIN <robinguillaume.pro@gmail.com>
# Date: 04/08/2017

if [ -z "$1" ]; then
    (>&2 echo -e "error: no name provided.\nUsage:\n\r./keys-generator name")
    exit 1
fi

key_name=$1
export LD_LIBRARY_PATH=$PWD/lib/bin

echo -e "----------------- Keys Generator -----------------"
echo -e "-      Algorithm (elliptic curve): secp256k1     -"
echo -e "--------------------------------------------------"

sudo openssl ecparam -name secp256k1 -genkey -noout | openssl ec -text -noout > key.tmp
cat key.tmp |grep pub -A 5 | tail -n +2 | tr -d '\n[:space:]:' | sed 's/^04//' > ${key_name}_key.pub
cat ${key_name}_key.pub | keccak-256sum -x -l | tr -d ' -' | tail -c 41 > ${key_name}_addr.txt
cat key.tmp |grep priv -A 3 | tail -n +2 | tr -d '\n[:space:]:' | sed 's/^00//' > ${key_name}_key
rm key.tmp
echo -e "Ethereum address: `cat ${key_name}_addr.txt`"
echo "All done."

# Ethereum (like) Keypair Generator It is a script bash that run some commands to generate a public, private key and compute the ethereum address of the new account.  ## Dependencies (only for creating account) - [geth](https://github.com/ethereum/go-ethereum/wiki/geth)

## Install dependencies
```
$> ./install-deps
```

## Clean installations of dependencies
```
$> ./install-deps clean
```

## Use keys generator
```
$> ./keys-generator myname [ -c or --create-account ] [ --keygen ]
```

## Output
- `${key_name}_acc.pub` - Public key for account in hex.
- `${key_name}_acc.key` - Private key for account in hex.
- `${key_name}_addr.txt` - Account address in hex.

If `-c` or `--create-account` is used:
- `${key_name}key` - Ciphered account.

If `--keygen` is used:
- `${key_name}.pub` - Node public key.
- `${key_name}.key` - Node private key.

## References
- [Create full ethereum wallet, keypair and address](https://kobl.one/blog/create-full-ethereum-keypair-and-address/), by Vincent KOBEL
- [Libkeccak](https://github.com/maandree/libkeccak)
- [sha3sum](https://github.com/maandree/sha3sum)
- [auto-auto-complete](https://github.com/maandree/auto-auto-complete)
- [argparser](https://github.com/maandree/argparser)

# Ethereum (like) Keypair Generator

It is a script bash that run some commands to generate a public, private key and compute the ethereum address of the new account.

## Dependencies (only for creating account)
- [geth](https://github.com/ethereum/go-ethereum/wiki/geth)

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
$> ./keys-generator myname [ -c or --create-account ]
```

## References
- [Create full ethereum wallet, keypair and address](https://kobl.one/blog/create-full-ethereum-keypair-and-address/), Vincent KOBEL
- [Libkeccak](https://github.com/maandree/libkeccak)
- [sha3sum](https://github.com/maandree/sha3sum)
- [auto-auto-complete](https://github.com/maandree/auto-auto-complete)
- [argparser](https://github.com/maandree/argparser)

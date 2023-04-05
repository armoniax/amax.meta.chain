# amax.meta.chain

This is the mother chain of Armonia Multi-chain Platform.

# Official Blockchain Explorer
- mainnet: https://amaxscan.io
- testnet: https://testnet.amaxscan.io

# How to build

## prepare the dev environment

1. Hardware requirement

- Minimum HW Spec: `4 Core CPU, 16 GB RAM`
- Preferred HW Spec: `8 Core CPU, 32 GB RAM`

2. Software requirement

**AMAX requires an LLVM version 7 through 11**

1. check clang version: `clang --version`
2. if the above result shows llvm thats' beyond the scope of 7 to 11, run the following to swith your llvm to version 8
```
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 100
```

## Download source code and make sure you pick the right branch/tag
```
git clone https://github.com/armoniax/amax.meta.chain.git
cd amax.meta.chain
git checkout $tag
git submodule update --init --recursive
```
## Build the code
```
./scripts/amax_build.sh -P
```

# How to install

## install amcli
```
./scripts/amax_install.sh
```

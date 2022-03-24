# Bios Boot Tutorial

The `bios-boot-tutorial.py` script simulates the AMAX bios boot sequence.

``Prerequisites``:

1. Python 3.x
2. CMake
3. git
4. g++
5. build-essentials
6. pip3
7. openssl
8. curl
9. jq
10. psmisc


``Steps``:

1. Install amax binaries by following the steps outlined in below tutorial
[Install amax binaries](https://github.com/armoniax/amax.releases/tree/main/amax.chain/README.md).

2. Install amax.cdt version 0.1.0 binaries by following the steps outlined in below tutorial
[Install amax.cdt binaries](https://github.com/armoniax/amax.releases/tree/main/amax.cdt/README.md).

3. Compile `amax.contracts` version 0.1.x.

```bash
$ cd ~
$ git clone https://github.com/armoniax/amax.contracts.git amax.contracts-0.1.x
$ cd ./amax.contracts-0.1.x/
$ git checkout v0.1.x
$ ./build.sh
$ cd ./build/contracts/
$ pwd

```

4. Make note of the directory where the contracts were compiled. 
The last command in the previous step printed on the bash console the contracts' directory, make note of it, we'll reference it from now on as `AMAX_OLD_CONTRACTS_DIRECTORY`.

5. Install amax.cdt version 0.1.x binaries by following the steps outlined in below tutorial, make sure you uninstall the previous one first.
[Install amax.cdt binaries](https://github.com/armoniax/amax.releases/tree/main/amax.cdt)

6. Compile `amax.contracts` sources version 0.1.1

```bash
$ cd ~
$ git clone https://github.com/armoniax/amax.contracts.git
$ cd ./amax.contracts/
$ git checkout v0.1.1
$ ./build.sh
$ cd ./build/contracts/
$ pwd

```

7. Make note of the directory where the contracts were compiled
The last command in the previous step printed on the bash console the contracts' directory, make note of it, we'll reference it from now on as `AMAX_CONTRACTS_DIRECTORY`


8. Launch the `bios-boot-tutorial.py` script. 
The command line to launch the script, make sure you replace `AMAX_OLD_CONTRACTS_DIRECTORY` and `AMAX_CONTRACTS_DIRECTORY` with actual directory paths.

```bash
$ cd ~
$ git clone https://github.com/armoniax/amax.chain.git
$ cd ./amax.chain/tutorials/bios-boot-tutorial/
$ python3 bios-boot-tutorial.py --amcli="amcli --wallet-url http://127.0.0.1:6666 " --amnod=amnod --amkey=amkey --contracts-dir="AMAX_CONTRACTS_DIRECTORY" -w -a
```

6. At this point, when the script has finished running without error, you have a functional AMAX based blockchain running locally with an latest version of `amax.system` contract, 31 block producers out of which 21 active, `amax` account resigned, 200k+ accounts with staked tokens, and votes allocated to each block producer. Enjoy exploring your freshly booted blockchain.

See [AMAX Documentation Wiki: Tutorial - Bios Boot](https://github.com/armoniax/amax.chain/wiki/Tutorial-Bios-Boot-Sequence) for additional information.
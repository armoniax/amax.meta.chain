#!/bin/bash

# variables
GIT_ROOT="$(dirname $BASH_SOURCE[0])/.."

echo 'Start update test contracts...'

files=($( cd "$GIT_ROOT/build/unittests/test-contracts" && find . -type f \( -name "*.wasm" -o -name "*.abi" \) | grep -v ./CMakeFiles ))

for f in ${files[*]} 
do
    cp -v "$GIT_ROOT/build/unittests/test-contracts/${f}" "$GIT_ROOT/unittests/test-contracts/${f}"
done

echo 'Done update test contracts.'


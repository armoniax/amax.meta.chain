#!/bin/bash
set -euo pipefail

. "${0%/*}/libfunctions.sh"

echo '+++ :minidisc: Installing AMAX'

if [[ $(apt-get --version 2>/dev/null) ]]; then # debian family packaging
    perform 'apt-get update'
    perform 'apt-get install -y /eos/*.deb'
elif [[ $(yum --version 2>/dev/null) ]]; then # RHEL family packaging
    perform 'yum check-update || :'
    perform 'yum install -y /eos/*.rpm'
elif [[ $(brew --version 2>/dev/null) ]]; then # homebrew packaging
    perform 'brew update'
    perform 'mkdir homebrew-amax'
    perform 'git init homebrew-amax'
    perform 'cp *.rb homebrew-amax'
    perform "sed -i.bk -e 's/url \".*\"/url \"http:\/\/127.0.0.1:7800\"/' homebrew-amax/*.rb"
    perform "pushd homebrew-amax && git add *.rb && git commit -m 'test it!' && popd"
    perform "brew tap amax/amax homebrew-amax"
    perform '{ python3 -m http.server 7800 & } && export HTTP_SERVER_PID=$!'
    perform 'sleep 20s'
    perform 'brew install amax'
    perform 'kill $HTTP_SERVER_PID'
else
    echo 'ERROR: Package manager not detected!'
    exit 3
fi

amnod --full-version

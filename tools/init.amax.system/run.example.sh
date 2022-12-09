
# ENV
cur_dir=$( dirname "${BASH_SOURCE[0]}" )
repos_dir="${cur_dir}/../.."
root_dir=${HOME}/data/amax/amax-devnet-local
amax_pubkey="AM8GzYfZx5oJSB5i2umry688HTBXRtGJZzjcTLAZGBJPjcjv51q1"
amax_privkey="5KHd1xZwqSGgEpcDnxLrZsEbjmvNs78W5fvpKrvc1KCTyrWTmpZ"
contracts_dir="$repos_dir/unittests/contracts"
apos_contracts_dir="$repos_dir/unittests/contracts/apos_version"
export PATH="${repos_dir}/build/bin:$PATH"

# Disable if the shell isn't interactive (avoids: tput: No value for $TERM and no -T specified)
if [[ $- == *i* ]]; then
  export COLOR_NC=$(tput sgr0) # No Color
  export COLOR_RED=$(tput setaf 1)
  export COLOR_GREEN=$(tput setaf 2)
  export COLOR_YELLOW=$(tput setaf 3)
fi

# 1.
# 1.1 [optional] init accounts with init.amax.system.py, or use existed accounts.json
# [NOTE] should backup the accounts.json
#python3 gen.accounts.py --new-account --num-producers 300 --num-users 500 --log-path "${root_dir}/run.log"

# 1.2 [optional] generate producers config
#mkdir -p ${root_dir}/config
# cat accounts.json | grep '"name":"producer' \
#     | awk -F '"' '{print "producer-name = "$4"\nsignature-provider = "$12"=KEY:"$8"\n"}' \
#     > ${root_dir}/config/producers.config.ini

function tester() {
    python3 ${cur_dir}/init.amax.system.py \
        --amcli='amcli --wallet-url http://127.0.0.1:6666 --url http://127.0.0.1:8888' \
        --log-path="${root_dir}"/run.log \
        --genesis="${root_dir}"/config/genesis.json \
        --contracts-dir="${contracts_dir}" \
        --public-key="${amax_pubkey}" --private-key="${amax_privkey}" \
        --num-producers 88 \
        $@

    [[ $? -ne 0 ]] && echo "${COLOR_RED}Execute init.amax.system.py failed!" && exit 1
}

#3. start test


# start wallet
echo "${COLOR_GREEN}Start wallet"
tester --kill --wallet --max-user-keys=500
# ps -ef | grep amkey

# deploy amax system contract

echo "${COLOR_GREEN}Deploy and initialize old version of amax system contract"
cmds=(
    '--sys'
    '--contracts'
    '--tokens'
    '--sys-contract'
    '--init-sys-contract'
    '--stake'
    '--reg-prod'
    '--vote --num-voters=30'
)
tester ${cmds[@]}


# upgrade apos version contracts
echo "${COLOR_GREEN}Deploy and initialize old version of amax system contract"
tester --upgrade-contracts --upgraded-contracts-dir=${apos_contracts_dir}

# upgrade producers first
echo "${COLOR_GREEN}Upgrade producers"
tester --reg-prod

# init apos
echo "${COLOR_GREEN}Init apos"
tester --init-apos --num-backup-producer 100

# vote
tester --vote --voter-started-idx=0 --num-voters=200
tester --proxy --voter-started-idx=201 --num-voters=10


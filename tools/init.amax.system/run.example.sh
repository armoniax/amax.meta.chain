
# ENV
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
work_dir=${work_dir:-"${SCRIPT_DIR}"}
repos_dir="${SCRIPT_DIR}/../.."
run_dir=${SCRIPT_DIR}/.run
amax_pubkey="AM8GzYfZx5oJSB5i2umry688HTBXRtGJZzjcTLAZGBJPjcjv51q1"
amax_privkey="5KHd1xZwqSGgEpcDnxLrZsEbjmvNs78W5fvpKrvc1KCTyrWTmpZ"
old_contracts_dir="$HOME/amax/amax.contracts/0.5/contracts"
apos_contracts_dir="$HOME/amax/amax.contracts/0.6/contracts"
export PATH="${repos_dir}/build/bin:$PATH"

# Make sure that all producers and signature-providers have been configured on the node

mkdir -p ${run_dir}
python3 ${SCRIPT_DIR}/init.amax.system.py \
    --amcli='amcli --wallet-url http://127.0.0.1:6666 --url http://127.0.0.1:8888' \
    --log-path="${run_dir}/run.log" \
    --wallet-dir="${run_dir}/wallet" \
    --accounts-path="${work_dir}/accounts.json" \
    --old-contracts-dir="${old_contracts_dir}" \
    --apos-contracts-dir="${apos_contracts_dir}" \
    --public-key="${amax_pubkey}" \
    --private-key="${amax_privkey}" \
    --num-producers=150 --producer-limit=150 --num-backup-producer 100 \
    --user-limit 100 --max-user-keys 100 --num-voters 100 \
    --all $@

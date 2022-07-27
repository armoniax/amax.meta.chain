#!/bin/bash
script_dir=$(
    cd $(dirname $0)
    pwd
)
build_dir="${script_dir}/../../../build"
export PATH="${build_dir}/bin:$PATH"

export COLOR_NC=$(tput sgr0) # No Color
export COLOR_RED=$(tput setaf 1)
export COLOR_GREEN=$(tput setaf 2)
export COLOR_YELLOW=$(tput setaf 3)
export COLOR_BLUE=$(tput setaf 4)
export COLOR_MAGENTA=$(tput setaf 5)
export COLOR_CYAN=$(tput setaf 6)
export COLOR_WHITE=$(tput setaf 7)

function info() {
    echo "${COLOR_GREEN}[info]$@${COLOR_NC}"
}

function error() {
    echo "${COLOR_RED}[error]$@${COLOR_NC}" && exit 1
}

function warn() {
    echo "${COLOR_YELLOW}[warn]$@${COLOR_NC}"
}

function deploy_contract() {
    contract=$1
    contract_dir=$2
    pubkey="${3:-AM6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV}"
    code_perm=${4:-1}

    info "create contract account ${contract}"
    amcli create account amax ${contract} ${pubkey} -p amax@active
    [[ $? -ne 0 ]] && warn "create contract account ${contract} failed"
    sleep 1

    info "set contract code ${contract}"
    amcli set contract ${contract} ${contract_dir} -p ${contract}@active
    [[ $? -ne 0 ]] && error "set contract code ${contract} failed"
    sleep 1

    [[ "X${code_perm}" != "X" ]] && [[ ${code_perm} -ne 0 ]] && amcli set account permission ${contract} active --add-code && sleep 1
}

features=(
    "f0af56d2c5a48d60a4a5b5c903edfb7db3a736a94ed589d0b797df33ff9d3e1d"#GET_SENDER
    "2652f5f96006294109b3dd0bbde63693f55324af452b799ee137a81a905eed25"#FORWARD_SETCODE
    "8ba52fe7a3956c5cd3a656a3174b931d3bb2abb45578befc59f283ecd816a405"#ONLY_BILL_FIRST_AUTHORIZER
    "ad9e3d8f650687709fd68f4b90b41f7d825a365b02c23a636cef88ac2ac00c43"#RESTRICT_ACTION_TO_SELF
    "68dcaa34c0517d19666e6b33add67351d8c5f69e999ca1e37931bc410a297428"#DISALLOW_EMPTY_PRODUCER_SCHEDULE
    "e0fb64b1085cc5538970158d05a009c24e276fb94e1a0bf6a528b48fbc4ff526"#FIX_LINKAUTH_RESTRICTION
    "ef43112c6543b88db2283a2e077278c315ae2c84719a8b25f25cc88565fbea99"#REPLACE_DEFERRED
    "4a90c00d55454dc5b059055ca213579c6ea856967712a56017487886a4d4cc0f"#NO_DUPLICATE_DEFERRED_ID
    "1a99a59d87e06e09ec5b028a9cbb7749b4a5ad8819004365d02dc4379a8b7241"#ONLY_LINK_TO_EXISTING_PERMISSION
    "4e7bf348da00a945489b2a681749eb56f5de00b900014e137ddae39f48f69d67"#RAM_RESTRICTIONS
    "4fca8bd82bbd181e714e283f83e1b45d95ca5af40fb89ad3977b653c448f78c2"#WEBAUTHN_KEY
    "299dcb6af692324b899b39f16d5a530a33062804e41f09dc97e9f156b4476707"#WTMSIG_BLOCK_SIGNATURES
)

info "activate features ..."
for f in ${features[@]}; do
    feature=$(echo ${f} | awk -F '#' '{print $1}')
    amcli system activate ${feature} -p amax
done
sleep 1

info "deploy amax.bios ..."
bios_contract_dir="${build_dir}/unittests/contracts/amax.bios"
amcli set contract amax ${bios_contract_dir} -p amax@active
sleep 1

contract_dir="${build_dir}/unittests/test-contracts/producer_change_test"
contract="bp.changer"

info "deploy contract ${contract}"
deploy_contract ${contract} ${contract_dir}

# upgrade contract
# amcli set contract ${contract} ${contract_dir} -p ${contract}@active

info "setpriv 1 for account ${contract}"
amcli push action amax setpriv '["'${contract}'", 1]' -p amax@active
sleep 1

pubkey="AM6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"

info "create account main.bp.1"
amcli create account amax "main.bp.1" ${pubkey} -p amax@active
info "create account backup.bp.1"
amcli create account amax "backup.bp.1" ${pubkey} -p amax@active
sleep 1

changes=$( cat <<EOF
{
    "changes": {
        "main_changes": {
            "producer_count": 1,
            "changes": [
                [
                    "amax",
                    [
                        "producer_authority_del",
                        {
                          "authority": null
                        }
                    ]
                ],
                [
                    "main.bp.1",
                    [
                        "producer_authority_add",
                        {
                            "authority": [
                                "block_signing_authority_v0",
                                {
                                    "threshold": 1,
                                    "keys": [
                                        {
                                            "key": "AM6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV",
                                            "weight": 1
                                        }
                                    ]
                                }
                            ]
                        }
                    ]
                ]
            ]
        },
        "backup_changes": {
            "producer_count": 1,
            "changes": [
                [
                    "backup.bp.1",
                    [
                        "producer_authority_add",
                        {
                            "authority": [
                                "block_signing_authority_v0",
                                {
                                    "threshold": 1,
                                    "keys": [
                                        {
                                            "key": "AM6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV",
                                            "weight": 1
                                        }
                                    ]
                                }
                            ]
                        }
                    ]
                ]
            ]
        }
    },
    "expected": null
}
EOF
)

info "change producers ..."
amcli push action ${contract} change "${changes}" -p ${contract}@active

#!/usr/bin/env python3

import argparse
import json
import numpy
import os
import random
import re
import subprocess
import sys
import time
from datetime import timedelta
from datetime import datetime

args = None
logFile = None

unlockTimeout = 999999999
fastUnstakeSystem = './fast.refund/amax.system/amax.system.wasm'

systemAccounts = [
    'amax.msig',
    'amax.names',
    'amax.ram',
    'amax.ramfee',
    'amax.stake',
    'amax.token',
    'amax.rex',
    'amax.reward',
]

def jsonArg(a):
    return " '" + json.dumps(a) + "' "

def run(args):
    print('init.amax.system.py:', args)
    logFile.write(args + '\n')
    if subprocess.call(args, shell=True):
        print('init.amax.system.py: exiting because of error')
        sys.exit(1)

def retry(args):
    count=3
    while True:
        count -= 1
        print('init.amax.system.py: ', args)
        logFile.write(args + '\n')
        if subprocess.call(args, shell=True):
            print('*** Retry')
        else:
            break
        if count == 0:
            sleep(3)
            count = 1

def background(args):
    print('init.amax.system.py:', args)
    logFile.write(args + '\n')
    return subprocess.Popen(args, shell=True)

def getOutput(args):
    print('init.amax.system.py:', args)
    logFile.write(args + '\n')
    proc = subprocess.Popen(args, shell=True, stdout=subprocess.PIPE)
    return proc.communicate()[0].decode('utf-8')

def getJsonOutput(args):
    return json.loads(getOutput(args))

def sleep(t):
    print('sleep', t, '...')
    time.sleep(t)
    print('resume')

def startWallet():
    run('killall amkey || true')
    sleep(1.5)
    run('rm -rf ' + os.path.abspath(args.wallet_dir))
    run('mkdir -p ' + os.path.abspath(args.wallet_dir))
    background(args.amkey + ' --unlock-timeout %d --http-server-address 127.0.0.1:6666 --wallet-dir %s' % (unlockTimeout, os.path.abspath(args.wallet_dir)))
    sleep(.4)
    run(args.amcli + 'wallet create --to-console')

def importKeys():
    run(args.amcli + 'wallet import --private-key ' + args.private_key)
    keys = {}
    for a in accounts:
        key = a['pvt']
        if not key in keys:
            if len(keys) >= args.max_user_keys:
                break
            keys[key] = True
            run(args.amcli + 'wallet import --private-key ' + key)
    for i in range(firstProducer, firstProducer + numProducers):
        a = accounts[i]
        key = a['pvt']
        if not key in keys:
            keys[key] = True
            run(args.amcli + 'wallet import --private-key ' + key)

def startNode(nodeIndex, account):
    dir = args.nodes_dir + ('%02d-' % nodeIndex) + account['name'] + '/'
    run('rm -rf ' + dir)
    run('mkdir -p ' + dir)
    otherOpts = ''.join(list(map(lambda i: '    --p2p-peer-address localhost:' + str(9000 + i), range(nodeIndex))))
    if not nodeIndex: otherOpts += (
        '    --plugin eosio::history_plugin'
        '    --plugin eosio::history_api_plugin'
    )
    cmd = (
        args.amnod +
        '    --max-irreversible-block-age -1'
        '    --max-transaction-time=1000'
        '    --contracts-console'
        '    --genesis-json ' + os.path.abspath(args.genesis) +
        '    --blocks-dir ' + os.path.abspath(dir) + '/blocks'
        '    --config-dir ' + os.path.abspath(dir) +
        '    --data-dir ' + os.path.abspath(dir) +
        '    --chain-state-db-size-mb 1024'
        '    --http-server-address 127.0.0.1:' + str(8000 + nodeIndex) +
        '    --p2p-listen-endpoint 127.0.0.1:' + str(9000 + nodeIndex) +
        '    --max-clients ' + str(maxClients) +
        '    --p2p-max-nodes-per-host ' + str(maxClients) +
        '    --enable-stale-production'
        '    --producer-name ' + account['name'] +
        '    --private-key \'["' + account['pub'] + '","' + account['pvt'] + '"]\''
        '    --plugin eosio::http_plugin'
        '    --plugin eosio::chain_api_plugin'
        '    --plugin eosio::chain_plugin'
        '    --plugin eosio::producer_api_plugin'
        '    --plugin eosio::producer_plugin' +
        otherOpts)
    with open(dir + 'stderr', mode='w') as f:
        f.write(cmd + '\n\n')
    background(cmd + '    2>>' + dir + 'stderr')

def startProducers(b, e):
    for i in range(b, e):
        startNode(i - b + 1, accounts[i])

def createSystemAccounts():
    for a in systemAccounts:
        run(args.amcli + 'create account amax ' + a + ' ' + args.public_key)

def intToCurrency(i):
    return '%d.%08d %s' % (i // 100000000, i % 100000000, args.symbol)

def allocateFunds(b, e):
    dist = numpy.random.pareto(1.161, e - b).tolist() # 1.161 = 80/20 rule
    dist.sort()
    dist.reverse()
    factor = 200_000_000 / sum(dist)
    total = 0
    for i in range(b, e):
        funds = round(factor * dist[i - b] * 100000000)
        if i >= firstProducer and i < firstProducer + numProducers:
            funds = max(funds, round(args.min_producer_funds * 100000000))
        total += funds
        accounts[i]['funds'] = funds
    print(accounts)
    print("allocate total founds: %f" %(total/100000000))
    return total

def createStakedAccounts(b, e):
    ramFunds = round(args.ram_funds * 100000000)
    configuredMinStake = round(args.min_stake * 100000000)
    maxUnstaked = round(args.max_unstaked * 100000000)
    for i in range(b, e):
        a = accounts[i]
        funds = a['funds']
        print('#' * 80)
        print('# %d/%d %s %s' % (i, e, a['name'], intToCurrency(funds)))
        print('#' * 80)
        if funds < ramFunds:
            print('skipping %s: not enough funds to cover ram' % a['name'])
            continue
        minStake = min(funds - ramFunds, configuredMinStake)
        unstaked = min(funds - ramFunds - minStake, maxUnstaked)
        stake = funds - ramFunds - unstaked
        stakeNet = round(stake / 2)
        stakeCpu = stake - stakeNet
        print('%s: total funds=%s, ram=%s, net=%s, cpu=%s, unstaked=%s' % (a['name'], intToCurrency(a['funds']), intToCurrency(ramFunds), intToCurrency(stakeNet), intToCurrency(stakeCpu), intToCurrency(unstaked)))
        assert(funds == ramFunds + stakeNet + stakeCpu + unstaked)
        retry(args.amcli + 'system newaccount --transfer amax %s %s --stake-net "%s" --stake-cpu "%s" --buy-ram "%s"   ' %
            (a['name'], a['pub'], intToCurrency(stakeNet), intToCurrency(stakeCpu), intToCurrency(ramFunds)))
        if unstaked:
            retry(args.amcli + 'transfer amax %s "%s"' % (a['name'], intToCurrency(unstaked)))

def regProducers(b, e):
    for i in range(b, e):
        a = accounts[i]
        retry(args.amcli + 'system regproducer ' + a['name'] + ' ' + a['pub'] + ' https://' + a['name'] + '.com' + '/' + a['pub'] + ' 0 8000')

def listProducers():
    run(args.amcli + 'system listproducers')

def vote(b, e):
    for i in range(b, e):
        voter = accounts[i]['name']
        k = args.num_producers_vote
        if k > numProducers:
            k = numProducers - 1
        prods = random.sample(range(firstProducer, firstProducer + numProducers), k)
        prods = ' '.join(map(lambda x: accounts[x]['name'], prods))
        retry(args.amcli + 'system voteproducer prods ' + voter + ' ' + prods)

def claimRewards():
    table = getJsonOutput(args.amcli + 'get table amax amax producers -l 100')
    times = []
    for row in table['rows']:
        if row['unpaid_blocks'] and not row['last_claim_time']:
            times.append(getJsonOutput(args.amcli + 'system claimrewards -j ' + row['owner'])['processed']['elapsed'])
    print('Elapsed time for claimrewards:', times)

def proxyVotes(b, e):
    vote(firstProducer, firstProducer + 1)
    proxy = accounts[firstProducer]['name']
    retry(args.amcli + 'system regproxy ' + proxy)
    sleep(1.0)
    for i in range(b, e):
        voter = accounts[i]['name']
        retry(args.amcli + 'system voteproducer proxy ' + voter + ' ' + proxy)

def updateAuth(account, permission, parent, controller):
    run(args.amcli + 'push action amax updateauth' + jsonArg({
        'account': account,
        'permission': permission,
        'parent': parent,
        'auth': {
            'threshold': 1, 'keys': [], 'waits': [],
            'accounts': [{
                'weight': 1,
                'permission': {'actor': controller, 'permission': 'active'}
            }]
        }
    }) + '-p ' + account + '@' + permission)

def resign(account, controller):
    updateAuth(account, 'owner', '', controller)
    updateAuth(account, 'active', 'owner', controller)
    sleep(1)
    run(args.amcli + 'get account ' + account)

def randomTransfer(b, e):
    for j in range(20):
        src = accounts[random.randint(b, e - 1)]['name']
        dest = src
        while dest == src:
            dest = accounts[random.randint(b, e - 1)]['name']
        run(args.amcli + 'transfer -f ' + src + ' ' + dest + ' "0.00010000 ' + args.symbol + '"' + ' || true')

def msigProposeReplaceSystem(proposer, proposalName):
    requestedPermissions = []
    for i in range(firstProducer, firstProducer + numProducers):
        requestedPermissions.append({'actor': accounts[i]['name'], 'permission': 'active'})
    trxPermissions = [{'actor': 'amax', 'permission': 'active'}]
    with open(fastUnstakeSystem, mode='rb') as f:
        setcode = {'account': 'amax', 'vmtype': 0, 'vmversion': 0, 'code': f.read().hex()}
    run(args.amcli + 'multisig propose ' + proposalName + jsonArg(requestedPermissions) +
        jsonArg(trxPermissions) + 'amax setcode' + jsonArg(setcode) + ' -p ' + proposer)

def msigApproveReplaceSystem(proposer, proposalName):
    for i in range(firstProducer, firstProducer + numProducers):
        run(args.amcli + 'multisig approve ' + proposer + ' ' + proposalName +
            jsonArg({'actor': accounts[i]['name'], 'permission': 'active'}) +
            '-p ' + accounts[i]['name'])

def msigExecReplaceSystem(proposer, proposalName):
    retry(args.amcli + 'multisig exec ' + proposer + ' ' + proposalName + ' -p ' + proposer)

def msigReplaceSystem():
    run(args.amcli + 'push action amax buyrambytes' + jsonArg(['amax', accounts[0]['name'], 200000]) + '-p amax')
    sleep(1)
    msigProposeReplaceSystem(accounts[0]['name'], 'fast.unstake')
    sleep(1)
    msigApproveReplaceSystem(accounts[0]['name'], 'fast.unstake')
    msigExecReplaceSystem(accounts[0]['name'], 'fast.unstake')

def genNewAccounts():
    with open(args.accounts_file, 'w') as f:
        producerNum = args.num_producers

        f.write('{\n')
        print("generating producers! num:", producerNum)
        f.write('    "producers": [\n')
        separator = ""
        for i in range(0, producerNum):
            x = getOutput(args.amcli + 'create key --to-console')
            r = re.match('Private key: *([^ \n]*)\nPublic key: *([^ \n]*)', x, re.DOTALL | re.MULTILINE)
            name = ''
            n = i
            for j in range(0, 4):
                name = chr(ord('a') + n % 26 ) + name
                n = int(n/26)
            name = "producer" + name
            #print(i, name)
            f.write('%s        {"name":"%s", "pvt":"%s", "pub":"%s"}' % (separator, name, r[1], r[2]))
            separator = ",\n"
        f.write('\n    ],\n')

        userNum = args.num_users
        print("generating users! num:", userNum)
        f.write('    "users": [\n')
        separator = ""
        for i in range(120_000, 120_000 + userNum):
            x = getOutput(args.amcli + 'create key --to-console')
            r = re.match('Private key: *([^ \n]*)\nPublic key: *([^ \n]*)', x, re.DOTALL | re.MULTILINE)
            name = 'user'
            for j in range(7, -1, -1):
                name += chr(ord('a') + ((i >> (j * 4)) & 15))
            #print(i, name)
            f.write('%s        {"name":"%s", "pvt":"%s", "pub":"%s"}' % (separator, name, r[1], r[2]))
            separator = ",\n"
        f.write('\n    ]\n')
        f.write('}\n')
def stepKillAll():
    run('killall amkey || true')
    sleep(1.5)
def stepStartWallet():
    startWallet()
    importKeys()
def stepStartBoot():
    startNode(0, {'name': 'amax', 'pvt': args.private_key, 'pub': args.public_key})
    sleep(1.5)
def stepInstallSystemContracts():
    run(args.amcli + 'set contract amax.token ' + args.contracts_dir + '/amax.token/')
    run(args.amcli + 'set contract amax.msig ' + args.contracts_dir + '/amax.msig/')
def stepCreateTokens():
    run(args.amcli + 'push action amax.token create \'["amax", "1000000000.00000000 %s"]\' -p amax.token' % (args.symbol))
    totalAllocation = allocateFunds(0, len(accounts))
    # run(args.amcli + 'push action amax.token issue \'["amax", "%s", "memo"]\' -p amax' % intToCurrency(totalAllocation))
    # allocate 50% of totalSupply
    run(args.amcli + 'push action amax.token issue \'["amax", "900000000.00000000 %s", "memo"]\' -p amax' % (args.symbol))
    sleep(1)
def stepSetSystemContract():

    # activate features
    # GET_SENDER
    retry(args.amcli + ' system activate "f0af56d2c5a48d60a4a5b5c903edfb7db3a736a94ed589d0b797df33ff9d3e1d" -p amax')
    # FORWARD_SETCODE
    retry(args.amcli + ' system activate "2652f5f96006294109b3dd0bbde63693f55324af452b799ee137a81a905eed25" -p amax')
    # ONLY_BILL_FIRST_AUTHORIZER
    retry(args.amcli + ' system activate "8ba52fe7a3956c5cd3a656a3174b931d3bb2abb45578befc59f283ecd816a405" -p amax')
    # RESTRICT_ACTION_TO_SELF
    retry(args.amcli + ' system activate "ad9e3d8f650687709fd68f4b90b41f7d825a365b02c23a636cef88ac2ac00c43" -p amax@active')
    # DISALLOW_EMPTY_PRODUCER_SCHEDULE
    retry(args.amcli + ' system activate "68dcaa34c0517d19666e6b33add67351d8c5f69e999ca1e37931bc410a297428" -p amax@active')
     # FIX_LINKAUTH_RESTRICTION
    retry(args.amcli + ' system activate "e0fb64b1085cc5538970158d05a009c24e276fb94e1a0bf6a528b48fbc4ff526" -p amax@active')
     # REPLACE_DEFERRED
    retry(args.amcli + ' system activate "ef43112c6543b88db2283a2e077278c315ae2c84719a8b25f25cc88565fbea99" -p amax@active')
    # NO_DUPLICATE_DEFERRED_ID
    retry(args.amcli + ' system activate "4a90c00d55454dc5b059055ca213579c6ea856967712a56017487886a4d4cc0f" -p amax@active')
    # ONLY_LINK_TO_EXISTING_PERMISSION
    retry(args.amcli + ' system activate "1a99a59d87e06e09ec5b028a9cbb7749b4a5ad8819004365d02dc4379a8b7241" -p amax@active')
    # RAM_RESTRICTIONS
    retry(args.amcli + ' system activate "4e7bf348da00a945489b2a681749eb56f5de00b900014e137ddae39f48f69d67" -p amax@active')
    # WEBAUTHN_KEY
    retry(args.amcli + ' system activate "4fca8bd82bbd181e714e283f83e1b45d95ca5af40fb89ad3977b653c448f78c2" -p amax@active')
    # WTMSIG_BLOCK_SIGNATURES
    retry(args.amcli + ' system activate "299dcb6af692324b899b39f16d5a530a33062804e41f09dc97e9f156b4476707" -p amax@active')
    sleep(1)

    # install amax.system latest version
    retry(args.amcli + 'set contract amax ' + args.contracts_dir + '/amax.system/')
    sleep(3)

    run(args.amcli + 'push action amax setpriv' + jsonArg(['amax.msig', 1]) + '-p amax@active')
    sleep(1)

def stepInitSystemContract():
    run(args.amcli + 'push action amax init' + jsonArg(['0', '8,' + args.symbol]) + '-p amax@active')
    sleep(1)
def stepCreateStakedAccounts():
    createStakedAccounts(0, len(accounts))
def stepRegProducers():
    regProducers(firstProducer, firstProducer + numProducers)
    sleep(1)
    listProducers()
def stepStartProducers():
    startProducers(firstProducer, firstProducer + numProducers)
    sleep(args.producer_sync_delay)
def stepVote():
    vote(args.voter_started_idx, args.voter_started_idx + args.num_voters)
    sleep(1)
    listProducers()
    sleep(5)
def stepProxyVotes():
    proxyVotes(args.voter_started_idx, args.voter_started_idx + args.num_voters)
def stepUpgradeSystemContracts():
    retry(args.amcli + 'set contract amax ' + args.upgraded_contracts_dir + '/amax.system/')
    retry(args.amcli + 'set contract amax.reward ' + args.upgraded_contracts_dir + '/amax.reward/')
def initApos():
    # APOS
    retry(args.amcli + ' system activate "adb712fab94945cc23d8da3efacfc695a0d57734fa7f53b280880b59734e2036" -p amax@active')
    sleep(1)

    run(args.amcli + 'push action amax initelects' + jsonArg([args.num_backup_producer]) + '-p amax@active')
    sleep(1)
    run(args.amcli + 'push action amax setinflation' + jsonArg(['1970-01-01T00:00:00', '0.20000000 AMAX']) + '-p amax@active')
    sleep(1)
def stepResign():
    resign('amax', 'amax.prods')
    for a in systemAccounts:
        resign(a, 'amax')
def stepTransfer():
    while True:
        randomTransfer(0, args.num_senders)
def stepLog():
    run('tail -n 60 ' + args.nodes_dir + '00-amax/stderr')

# Command Line Arguments

parser = argparse.ArgumentParser()

commands = [
    ('k', 'kill',               stepKillAll,                False,    "Kill all amnod and amkey processes"),
    ('w', 'wallet',             stepStartWallet,            True,    "Start amkey, create wallet, fill with keys"),
    ('s', 'sys',                createSystemAccounts,       True,    "Create system accounts (amax.*)"),
    ('c', 'contracts',          stepInstallSystemContracts, True,    "Install system contracts (token, msig)"),
    ('t', 'tokens',             stepCreateTokens,           True,    "Create tokens"),
    ('S', 'sys-contract',       stepSetSystemContract,      True,    "Set system contract"),
    ('I', 'init-sys-contract',  stepInitSystemContract,     True,    "Initialiaze system contract"),
    ('T', 'stake',              stepCreateStakedAccounts,   True,    "Create staked accounts"),
    ('p', 'reg-prod',           stepRegProducers,           True,    "Register producers"),
    ('v', 'vote',               stepVote,                   True,    "Vote for producers"),
    ('R', 'claim',              claimRewards,               True,    "Claim rewards"),
    ('x', 'proxy',              stepProxyVotes,             True,    "Proxy votes"),

    ('U', 'upgrade-contracts',  stepUpgradeSystemContracts, True,    "Upgrade contracts"),
    ('', 'init-apos',           initApos,                   True,    "Init apos"),
    ('q', 'resign',             stepResign,                 True,    "Resign amax"),
    ('m', 'msg-replace',        msigReplaceSystem,          False,   "Replace system contract using msig"),
    ('X', 'xfer',               stepTransfer,               False,   "Random transfer tokens (infinite loop)"),
    ('l', 'log',                stepLog,                    True,    "Show tail of node's log"),
    ('N', 'new-account',        genNewAccounts,             False,    "Generate new accounts"),
]

parser.add_argument('--public-key', metavar='', help="AMAX Public Key", default='AM6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV', dest="public_key")
parser.add_argument('--private-key', metavar='', help="AMAX Private Key", default='5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3', dest="private_key")
parser.add_argument('--amcli', metavar='', help="Amcli command", default='amcli --wallet-url http://127.0.0.1:6666 --url http://127.0.0.1:8888 ')
parser.add_argument('--amnod', metavar='', help="Path to amnod binary", default='amnod')
parser.add_argument('--amkey', metavar='', help="Path to amkey binary", default='amkey')
parser.add_argument('--contracts-dir', metavar='', help="Path to latest contracts directory", default='${HOME}/amax/contracts/amax.contracts')
parser.add_argument('--upgraded-contracts-dir', metavar='', help="Path to upgraded contracts directory", default='${HOME}/amax/contracts/upgraded.amax.contracts')
# parser.add_argument('--nodes-dir', metavar='', help="Path to nodes directory", default='./nodes/')
parser.add_argument('--genesis', metavar='', help="Path to genesis.json", default="./genesis.json")
parser.add_argument('--wallet-dir', metavar='', help="Path to wallet directory", default='./wallet/')
parser.add_argument('--log-path', metavar='', help="Path to log file", default='./output.log')
parser.add_argument('--symbol', metavar='', help="The amax.system symbol", default='AMAX')
parser.add_argument('--user-limit', metavar='', help="Max number of users. (0 = no limit)", type=int, default=3000)
parser.add_argument('--max-user-keys', metavar='', help="Maximum user keys to import into wallet", type=int, default=10)
parser.add_argument('--ram-funds', metavar='', help="How much funds for each user to spend on ram", type=float, default=1.0)
parser.add_argument('--min-stake', metavar='', help="Minimum stake before allocating unstaked funds", type=float, default=0.9)
parser.add_argument('--max-unstaked', metavar='', help="Maximum unstaked funds", type=float, default=10)
parser.add_argument('--producer-limit', metavar='', help="Maximum number of producers. (0 = no limit)", type=int, default=0)
parser.add_argument('--min-producer-funds', metavar='', help="Minimum producer funds", type=float, default=1000.00000000)
parser.add_argument('--num-producers', metavar='', help="Number of producers to generate", type=int, default=21)
parser.add_argument('--num-users', metavar='', help="Number of users", type=int, default=10)
parser.add_argument('--num-producers-vote', metavar='', help="Number of producers for which each user votes", type=int, default=20)
parser.add_argument('--voter-started-idx', metavar='', help="Started index of voters", type=int, default=0)
parser.add_argument('--num-voters', metavar='', help="Number of voters", type=int, default=10)
parser.add_argument('--num-backup-producer', metavar='', help="Number of backup producers", type=int, default=100)
parser.add_argument('--num-senders', metavar='', help="Number of users to transfer funds randomly", type=int, default=10)
parser.add_argument('--producer-sync-delay', metavar='', help="Time (s) to sleep to allow producers to sync", type=int, default=80)
parser.add_argument('-a', '--all', action='store_true', help="Do everything marked with (*)")
# parser.add_argument('-H', '--http-port', type=int, default=8000, metavar='', help='HTTP port for amcli')

for (flag, command, function, inAll, help) in commands:
    prefix = ''
    if inAll: prefix += '*'
    if prefix: help = '(' + prefix + ') ' + help
    if flag:
        parser.add_argument('-' + flag, '--' + command, action='store_true', help=help, dest=command)
    else:
        parser.add_argument('--' + command, action='store_true', help=help, dest=command)

args = parser.parse_args()

# args.amcli += '--url http://127.0.0.1:%d ' % args.http_port
args.amcli += " "

logFile = open(args.log_path, 'a')

logFile.write('\n\n' + '*' * 80 + '\n\n\n')

with open('accounts.json') as f:
    a = json.load(f)
    if args.user_limit:
        del a['users'][args.user_limit:]
    if args.producer_limit:
        del a['producers'][args.producer_limit:]
    firstProducer = len(a['users'])
    numProducers = len(a['producers'])
    accounts = a['users'] + a['producers']

maxClients = numProducers + 10

haveCommand = False
for (flag, command, function, inAll, help) in commands:
    if getattr(args, command) or inAll and args.all:
        if function:
            haveCommand = True
            function()
if not haveCommand:
    print('init.amax.system.py: Tell me what to do. -a does almost everything. -h shows options.')

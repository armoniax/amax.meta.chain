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

args = None
logFile = None

def getOutput(args):
    print('gen.accounts.py:', args)
    logFile.write(args + '\n')
    proc = subprocess.Popen(args, shell=True, stdout=subprocess.PIPE)
    return proc.communicate()[0].decode('utf-8')

def getJsonOutput(args):
    return json.loads(getOutput(args))

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

# Command Line Arguments

parser = argparse.ArgumentParser()

commands = [
    ('N', 'new-account',        genNewAccounts,         True,    "Generate new accounts"),
]


parser.add_argument('--amcli', metavar='', help="Amcli command", default='amcli --wallet-url http://127.0.0.1:6666 --url http://127.0.0.1:8888 ')
# parser.add_argument('--nodes-dir', metavar='', help="Path to nodes directory", default='./nodes/')
parser.add_argument('--log-path', metavar='', help="Path to log file", default='./output.log')
parser.add_argument('--accounts-file', metavar='', help="Path to generating file", default='./accounts.json')
parser.add_argument('--num-producers', metavar='', help="Number of producers to generate", type=int, default=21)
parser.add_argument('--num-users', metavar='', help="Number of users", type=int, default=10)
parser.add_argument('-a', '--all', action='store_true', help="Do everything marked with (*)")

for (flag, command, function, inAll, help) in commands:
    prefix = ''
    if inAll: prefix += '*'
    if prefix: help = '(' + prefix + ') ' + help
    if flag:
        parser.add_argument('-' + flag, '--' + command, action='store_true', help=help, dest=command)
    else:
        parser.add_argument('--' + command, action='store_true', help=help, dest=command)

args = parser.parse_args()

args.amcli += " "

logFile = open(args.log_path, 'a')

logFile.write('\n\n' + '*' * 80 + '\n\n\n')

haveCommand = False
for (flag, command, function, inAll, help) in commands:
    if getattr(args, command) or inAll and args.all:
        if function:
            haveCommand = True
            function()
if not haveCommand:
    print('gen.accounts.py: Tell me what to do. -a does almost everything. -h shows options.')

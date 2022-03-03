#!/usr/bin/env python3

# This script tests that amcli launches amkey automatically when amkey is not
# running yet.

import subprocess


def run_amcli_wallet_command(command: str, no_auto_amkey: bool):
    """Run the given amcli command and return subprocess.CompletedProcess."""
    args = ['./programs/amcli/amcli']

    if no_auto_amkey:
        args.append('--no-auto-amkey')

    args += 'wallet', command

    return subprocess.run(args,
                          check=False,
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.PIPE)


def stop_amkey():
    """Stop the default amkey instance."""
    run_amcli_wallet_command('stop', no_auto_amkey=True)


def check_amcli_stderr(stderr: bytes, expected_match: bytes):
    if expected_match not in stderr:
        raise RuntimeError("'{}' not found in {}'".format(
            expected_match.decode(), stderr.decode()))


def amkey_auto_launch_test():
    """Test that amcli auto-launching works but can be optionally inhibited."""
    stop_amkey()

    # Make sure that when '--no-auto-amkey' is given, amkey is not started by
    # amcli.
    completed_process = run_amcli_wallet_command('list', no_auto_amkey=True)
    assert completed_process.returncode != 0
    check_amcli_stderr(completed_process.stderr, b'Failed to connect to amkey')

    # Verify that amkey auto-launching works.
    completed_process = run_amcli_wallet_command('list', no_auto_amkey=False)
    if completed_process.returncode != 0:
        raise RuntimeError("Expected that amkey would be started, "
                           "but got an error instead: {}".format(
                               completed_process.stderr.decode()))
    check_amcli_stderr(completed_process.stderr, b'launched')


try:
    amkey_auto_launch_test()
finally:
    stop_amkey()

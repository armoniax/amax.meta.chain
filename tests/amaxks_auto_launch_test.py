#!/usr/bin/env python3

# This script tests that amaxcl launches amaxks automatically when amaxks is not
# running yet.

import subprocess


def run_amaxcl_wallet_command(command: str, no_auto_amaxks: bool):
    """Run the given amaxcl command and return subprocess.CompletedProcess."""
    args = ['./programs/amaxcl/amaxcl']

    if no_auto_amaxks:
        args.append('--no-auto-amaxks')

    args += 'wallet', command

    return subprocess.run(args,
                          check=False,
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.PIPE)


def stop_amaxks():
    """Stop the default amaxks instance."""
    run_amaxcl_wallet_command('stop', no_auto_amaxks=True)


def check_amaxcl_stderr(stderr: bytes, expected_match: bytes):
    if expected_match not in stderr:
        raise RuntimeError("'{}' not found in {}'".format(
            expected_match.decode(), stderr.decode()))


def amaxks_auto_launch_test():
    """Test that keos auto-launching works but can be optionally inhibited."""
    stop_amaxks()

    # Make sure that when '--no-auto-amaxks' is given, amaxks is not started by
    # amaxcl.
    completed_process = run_amaxcl_wallet_command('list', no_auto_amaxks=True)
    assert completed_process.returncode != 0
    check_amaxcl_stderr(completed_process.stderr, b'Failed to connect to amaxks')

    # Verify that amaxks auto-launching works.
    completed_process = run_amaxcl_wallet_command('list', no_auto_amaxks=False)
    if completed_process.returncode != 0:
        raise RuntimeError("Expected that amaxks would be started, "
                           "but got an error instead: {}".format(
                               completed_process.stderr.decode()))
    check_amaxcl_stderr(completed_process.stderr, b'launched')


try:
    amaxks_auto_launch_test()
finally:
    stop_amaxks()

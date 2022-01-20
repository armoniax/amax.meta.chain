#!/usr/bin/env python3

# This script tests that amaxcl launches amaxkm automatically when amaxkm is not
# running yet.

import subprocess


def run_amaxcl_wallet_command(command: str, no_auto_amaxkm: bool):
    """Run the given amaxcl command and return subprocess.CompletedProcess."""
    args = ['./programs/amaxcl/amaxcl']

    if no_auto_amaxkm:
        args.append('--no-auto-amaxkm')

    args += 'wallet', command

    return subprocess.run(args,
                          check=False,
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.PIPE)


def stop_amaxkm():
    """Stop the default amaxkm instance."""
    run_amaxcl_wallet_command('stop', no_auto_amaxkm=True)


def check_amaxcl_stderr(stderr: bytes, expected_match: bytes):
    if expected_match not in stderr:
        raise RuntimeError("'{}' not found in {}'".format(
            expected_match.decode(), stderr.decode()))


def amaxkm_auto_launch_test():
    """Test that keos auto-launching works but can be optionally inhibited."""
    stop_amaxkm()

    # Make sure that when '--no-auto-amaxkm' is given, amaxkm is not started by
    # amaxcl.
    completed_process = run_amaxcl_wallet_command('list', no_auto_amaxkm=True)
    assert completed_process.returncode != 0
    check_amaxcl_stderr(completed_process.stderr, b'Failed to connect to amaxkm')

    # Verify that amaxkm auto-launching works.
    completed_process = run_amaxcl_wallet_command('list', no_auto_amaxkm=False)
    if completed_process.returncode != 0:
        raise RuntimeError("Expected that amaxkm would be started, "
                           "but got an error instead: {}".format(
                               completed_process.stderr.decode()))
    check_amaxcl_stderr(completed_process.stderr, b'launched')


try:
    amaxkm_auto_launch_test()
finally:
    stop_amaxkm()

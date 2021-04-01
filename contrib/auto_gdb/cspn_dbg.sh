#!/bin/bash
# use testnet settings,  if you need mainnet,  use ~/.cspncore/cspnd.pid file instead
cspn_pid=$(<~/.cspncore/testnet3/cspnd.pid)
sudo gdb -batch -ex "source debug.gdb" cspnd ${cspn_pid}

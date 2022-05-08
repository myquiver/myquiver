#!/bin/bash

set -eux

# Create and mount swapfile to ensure enough memory to build mysql-server
if [[ ! -e /tmp/swapfile ]]; then
  sudo dd if=/dev/zero of=/tmp/swapfile bs=128M count=32
  sudo chmod 600 /tmp/swapfile
  sudo mkswap /tmp/swapfile
fi
if ! sudo swapon -s | grep /tmp/swapfile > /dev/null; then
  sudo swapon /tmp/swapfile
fi

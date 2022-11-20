#!/bin/bash

set -eux

# clone mysql-server source to build myquiver
if [[ ! -d /workspaces/mysql-server ]]; then
  git clone --depth=1 -b mysql-8.0.27 https://github.com/mysql/mysql-server /workspaces/mysql-server
fi

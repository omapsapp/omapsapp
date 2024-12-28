#!/usr/bin/env bash
#
# Please run this script to configure the repository after cloning it.
#

set -euo pipefail

echo "Configuring the repository for development."

if [ ! -d 3party/boost/tools ]; then
  git submodule update --init --recursive
fi
pushd 3party/boost/
./bootstrap.sh
./b2 headers
popd
echo "The repository is configured for development."

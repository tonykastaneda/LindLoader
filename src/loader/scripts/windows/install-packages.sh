#!/usr/bin/env bash

echo "Install Packages Running..."
sudo apt update && sudo apt upgrade -y
sudo apt -y install --no-install-recommends cmake mingw-w64 mingw-w64-i686-dev zip unzip make

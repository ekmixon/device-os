#!/bin/bash
#
# Prerequisites:
#   git
#   python
#   pip
KERNEL_NAME=$(uname -s)

# Fail on errors
set -e

# MacOS Support
if [ "${KERNEL_NAME}" == "Darwin" ]; then
  brew install git python

# Debian Support
else
  (apt-get -qq update &&
  apt-get -qq install git python3-pip libxml2-dev libxslt-dev)
fi

python3 --version

# get the last version of pip compatible with python 3.5
# pip3 install --upgrade pip==20.3.4 

pip3 --version

pip3 install requests

# install a specific gcovr version. May not work if that version isn't compatible with the pip3 version
pip3 install gcovr==5.0

# install latest stable gcovr version
# pip3 install gcovr

# install gcovr latest develop branch directly from github
# pip3 install git+https://github.com/gcovr/gcovr.git

# FIXME: MarkupSafe is broken on python3.5
ln -s /usr/local/lib/python3.5/dist-packages/MarkupSafe-0.0.0.dist-info /usr/local/lib/python3.5/dist-packages/MarkupSafe-2.0.0.dist-info

gcovr --version

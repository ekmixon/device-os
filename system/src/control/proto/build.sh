#!/bin/bash

# Note for macOS users:
#
# 1. The following dependencies are required to build the protocol files using the nanopb plugin:
#
# brew install protobuf python
# pip2 install protobuf
#
# 2. Make sure your system Python can find Python modules installed via Homebrew:
#
# mkdir -p ~/Library/Python/2.7/lib/python/site-packages
# echo 'import site; site.addsitedir("/usr/local/lib/python2.7/site-packages")' >> ~/Library/Python/2.7/lib/python/site-packages/homebrew.pth

command -v protoc &> /dev/null || { echo "protoc not installed" 2>&1; exit 1; }

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

FIRMWARE_ROOT_DIR="${DIR}/../../../.."

PROTO_DIR="${FIRMWARE_ROOT_DIR}/proto/control"
NANOPB_PATH="${FIRMWARE_ROOT_DIR}/third_party/nanopb/nanopb"
PROTOC_NANOPB_PLUGIN="${NANOPB_PATH}/generator/protoc-gen-nanopb"
PROTOC_INCLUDE_PATH="
  -I${PROTO_DIR} \
  -I${NANOPB_PATH}/generator \
  -I${NANOPB_PATH}/generator/proto"

gen_proto() {
  protoc ${PROTOC_INCLUDE_PATH} --plugin=protoc-gen-nanopb=${PROTOC_NANOPB_PLUGIN} --nanopb_out=${DIR} "$1"
}

gen_proto "${PROTO_DIR}/extensions.proto"
gen_proto "${PROTO_DIR}/common.proto"
gen_proto "${PROTO_DIR}/control.proto"
gen_proto "${PROTO_DIR}/config.proto"
gen_proto "${PROTO_DIR}/wifi.proto"
gen_proto "${PROTO_DIR}/wifi_new.proto"
gen_proto "${PROTO_DIR}/cellular.proto"
gen_proto "${PROTO_DIR}/network.proto"
gen_proto "${PROTO_DIR}/storage.proto"
gen_proto "${PROTO_DIR}/mesh.proto"
gen_proto "${PROTO_DIR}/cloud.proto"
gen_proto "${PROTO_DIR}/internal.proto"

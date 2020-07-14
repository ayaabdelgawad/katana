#!/bin/bash
#
# This script sets up a development environment on MacOS.

set -eu

REPO_ROOT=$(cd "$(dirname $0)"/..; pwd)

bash -x "${REPO_ROOT}/.github/workflows/setup_macos.sh"

"${REPO_ROOT}/.github/workflows/setup_conan.sh"

# If you want a build directory that is a subdir of Katana root
# mkdir build
# cd build
# cmake ../ -DGALOIS_AUTO_CONAN=on -DGALOIS_ENABLE_DIST=on

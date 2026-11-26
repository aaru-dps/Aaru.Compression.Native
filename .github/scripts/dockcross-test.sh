#!/usr/bin/env bash
# Builds the library and the test suite with a dockcross toolchain (the same
# images build.sh uses) and, unless RUN is empty, runs the tests inside the
# container.
#
# Usage: dockcross-test.sh <image> <target> [run-command]
#   image        dockcross image name, e.g. linux-s390x
#   target       build directory name under build/, e.g. ci-linux-s390x
#   run-command  command executed in build/<target>/tests, e.g. "qemu-s390x ./tests_run".
#                Omit it to only build.
# Environment:
#   CMAKE_EXTRA  extra arguments for cmake (e.g. -DAARU_MUSL=1)
#   LINK_FIX     gold   -> strip -fuse-ld=gold from link lines (Android)
#                soname -> strip -soname,... from link lines (Windows ARM)

set -euo pipefail

IMAGE=$1
TARGET=$2
RUN=${3:-}

cd "$(dirname "$0")/../.."

mkdir -p docker "build/${TARGET}"
SCRIPT="docker/dockcross-ci-${IMAGE}"
docker run --rm "dockcross/${IMAGE}" > "${SCRIPT}"
chmod +x "${SCRIPT}"

"${SCRIPT}" bash -c "cd build/${TARGET} && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release ${CMAKE_EXTRA:-} ../.."

case "${LINK_FIX:-}" in
    gold)   PATTERN='s/\-fuse-ld=gold//g' ;;
    soname) PATTERN='s/\-soname,libAaru\.Compression\.Native\.so//g' ;;
    "")     PATTERN='' ;;
    *)      echo "Unknown LINK_FIX ${LINK_FIX}" >&2; exit 1 ;;
esac

if [[ -n ${PATTERN} ]]; then
    find "build/${TARGET}" -name link.txt -print0 | xargs -0 sed -i -e "${PATTERN}"
fi

"${SCRIPT}" bash -c "cd build/${TARGET} && make -j\$(nproc) Aaru.Compression.Native tests_run"

if [[ -n ${RUN} ]]; then
    "${SCRIPT}" bash -c "cd build/${TARGET}/tests && ${RUN}"
fi

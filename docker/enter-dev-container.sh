#!/usr/bin/env  bash
set -euo pipefail
IMAGE_NAME="zephyr-build"
IMAGE_NAME_DEV="zephyr-dev"

SERIAL_DEVICE="/dev/tbeam0"
REAL_SERIAL_DEVICE="$(readlink -f ${SERIAL_DEVICE})"
SERIAL_DEVICE_GID=$(stat -Lc %g ${REAL_SERIAL_DEVICE})

REPO_DIR="$(git rev-parse --show-toplevel 2>/dev/null)"
REPO_NAME="$(basename ${REPO_DIR})"

SSH_FLAGS=()
if [[ -n "${SSH_AUTH_SOCK:-}" && -S "${SSH_AUTH_SOCK}" ]]; then
    SSH_FLAGS+=(-v "${SSH_AUTH_SOCK}:${SSH_AUTH_SOCK}" -e SSH_AUTH_SOCK)
    # Add the agent's group to avoid EACCES on the socket
    if command -v stat >/dev/null 2>&1; then
        SSH_GID="$(stat -c %g "$SSH_AUTH_SOCK" 2>/dev/null || echo "")"
        [[ -n "$SSH_GID" ]] && SSH_FLAGS+=(--group-add "$SSH_GID")
    fi
fi

MOUNT_SUFFIX=""
if [[ -f /sys/fs/selinux/enforce ]] && [[ "$(cat /sys/fs/selinux/enforce)" != "0" ]]; then
    MOUNT_SUFFIX=":Z"
fi

# Build image with your host UID/GID so files are owned by you
docker build \
    --build-arg USER_NAME=$(id -un) \
    --build-arg USER_UID=$(id -u) \
    --build-arg USER_GID=$(id -g) \
    -f Dockerfile.build \
    -t ${IMAGE_NAME} \
    .

docker build \
    --build-arg USER_NAME=$(id -un) \
    --build-arg USER_UID=$(id -u) \
    --build-arg USER_GID=$(id -g) \
    --build-arg SERIAL_DEVICE=${SERIAL_DEVICE} \
    --build-arg SERIAL_DEVICE_GID=${SERIAL_DEVICE_GID} \
    -f Dockerfile.dev \
    -t ${IMAGE_NAME_DEV} \
    .

# Run the container:
# - --init for proper signal handling
# - --user to ensure writes land as you (matches build-args above)
# - HOME set explicitly (avoids weird $HOME= / when overriding --user)
docker run --rm -it --init \
    --user $(id -u):$(id -g) \
    "${SSH_FLAGS[@]}" \
    --privileged \
    --device="${REAL_SERIAL_DEVICE}:${SERIAL_DEVICE}" \
    --group-add "${SERIAL_DEVICE_GID}" \
    -v "/dev/bus/usb/:/dev/bus/usb" \
    -v "${REPO_DIR}:/workspaces/${REPO_NAME}" \
    -w "/workspaces/${REPO_NAME}" \
    ${IMAGE_NAME_DEV}:latest bash

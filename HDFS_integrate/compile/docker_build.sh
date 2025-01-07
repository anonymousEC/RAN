#!/usr/bin/env bash
set -e               # exit on error
cd "$(dirname "$0")" # change to script directory

DOCKER_DIR="."
DOCKER_FILE="Dockerfile"
docker build -t hadoop-build -f "$DOCKER_FILE" "$DOCKER_DIR"

USER_NAME=${SUDO_USER:=$USER}
docker build -t "hadoop-build_ran" - <<UserSpecificDocker
FROM hadoop-build
ENV HOME /home/${USER_NAME}
UserSpecificDocker
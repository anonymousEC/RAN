#!/usr/bin/env bash

# If this environment variable is empty, docker will be started
# in non-interactive mode
DOCKER_INTERACTIVE_RUN=${DOCKER_INTERACTIVE_RUN-"-i -t"}

# Optionally specify volume options for SELinux
V_OPTS=${V_OPTS:z}

# The user ID and name to run the container as
USER_NAME=${USER_NAME:-${SUDO_USER:=$USER}}
USER_ID=${USER_ID:-$(id -u "${USER_NAME}")}

# Define the path to isa-l directory on the host
ISAL_LIB_HOST_PATH=/home/ecRepair/RAN/tools/isa-l

# Run the Docker container
docker run $DOCKER_INTERACTIVE_RUN \
  -v "${PWD}:/home/${USER_NAME}/hadoop${V_OPTS}" \
  -w "/home/${USER_NAME}/hadoop" \
  -v "${HOME}/.m2:/home/${USER_NAME}/.m2${V_OPTS}" \
  -v "${HOME}/.gnupg:/home/${USER_NAME}/.gnupg${V_OPTS}" \
  -v "${ISAL_LIB_HOST_PATH}:/home/${USER_NAME}/isa-l${V_OPTS}" \
  hadoop-build_ran "$@"
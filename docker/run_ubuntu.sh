#!/usr/bin/env bash

DOCKER_IMAGE_NAME="organicmaps/ubuntu"

THIS_SCRIPT_PATH=$(cd "$(dirname "$0")"; pwd -P) || exit 1
OM_PATH="${THIS_SCRIPT_PATH}/../"

# Need to change directory to OM_PATH to be able to use it inside the container
cd "${OM_PATH}" || exit 1
docker build -t ${DOCKER_IMAGE_NAME} -f "${OM_PATH}"/docker/ubuntu/Dockerfile .
# Switch back to the original directory
cd - || exit 1

docker run \
  -it \
  --hostname organicmaps \
  -v "${OM_PATH}":"/organicmaps" \
  --workdir "/organicmaps" \
  ${DOCKER_IMAGE_NAME}

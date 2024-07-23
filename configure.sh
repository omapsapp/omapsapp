#!/usr/bin/env bash
# Please run this script to configure the repository after cloning it.

# When configuring with private repository, the following override hierarchy is used:
# - commandline parameters - most specific, always wins.
# - stdin parameters.
# - saved repository - least specific, if present.
# - fallback to opensource mode.

# Stop on the first error.
set -e -u

BASE_PATH=$(cd "$(dirname "$0")"; pwd)

DEFAULT_PRIVATE_HEADER="$BASE_PATH/private_default.h"
PRIVATE_HEADER="private.h"
PRIVATE_PROPERTIES="android/app/secure.properties"
PRIVATE_NETWORK_CONFIG="android/app/src/main/res/xml/network_security_config.xml"
PRIVATE_GOOGLE_SERVICES="android/app/google-services.json"

SAVED_PRIVATE_REPO_FILE="$BASE_PATH/.private_repository_url"
SAVED_PRIVATE_BRANCH_FILE="$BASE_PATH/.private_repository_branch"
TMP_REPO_DIR="$BASE_PATH/.tmp.private.repo"

usage() {
  echo "This tool configures Organic Maps for an opensource build by default"
  echo "and bootstraps the Boost submodule after that."
  echo
  echo "To e.g. publish in app stores populate following configs with your own private keys etc."
  echo "  $PRIVATE_HEADER"
  echo "  $PRIVATE_PROPERTIES"
  echo "  $PRIVATE_NETWORK_CONFIG"
  echo "  $PRIVATE_GOOGLE_SERVICES"
  echo "The tool can copy over the necessary configs from a given private repo and a branch"
  echo "(it copies all files except README.md)."
  echo "It remembers the repo and the branch to pull the config changes automatically on next run."
  echo
  echo "Usage:"
  echo "  $0 private_repo_url [private_repo_branch]  - copy configs from a private repo (master is the default branch)"
  echo "  echo 'private_repo_url [private_repo_branch]' | $0  - alternate invocation for private repo configuration"
  echo "  $0  - use a saved repo and a branch if present or default to an opensource build configs"
  echo ""
}

setup_opensource() {
  echo "Initializing repository with default values in Open-Source mode."
  cat "$DEFAULT_PRIVATE_HEADER" > "$BASE_PATH/$PRIVATE_HEADER"
  echo 'ext {
  spropStoreFile = "debug.keystore"
  spropStorePassword = "12345678"
  spropKeyAlias = "debug"
  spropKeyPassword = "12345678"
}
' > "$BASE_PATH/$PRIVATE_PROPERTIES"
  echo '<?xml version="1.0" encoding="utf-8"?>
<network-security-config>
  <base-config>
    <trust-anchors>
      <!-- Certificates are required for Android 7 and below. See the link for details:
           https://community.letsencrypt.org/t/letsencrypt-certificates-fails-on-android-phones-running-android-7-or-older/205686 -->
      <certificates src="@raw/isrgrootx1" />
      <certificates src="@raw/globalsignr4" />
      <certificates src="@raw/gtsrootr1" />
      <certificates src="@raw/gtsrootr2" />
      <certificates src="@raw/gtsrootr3" />
      <certificates src="@raw/gtsrootr4" />
      <certificates src="system" />
    </trust-anchors>
  </base-config>
</network-security-config>
' > "$BASE_PATH/$PRIVATE_NETWORK_CONFIG"
  rm -f "$BASE_PATH/$PRIVATE_GOOGLE_SERVICES"
}

# Clone the private repo and copy all of its files (except README.md) into the main repo
setup_private() {
  echo "Copying private configuration files from the repo '$PRIVATE_REPO', branch '$PRIVATE_BRANCH'"
  set -x
  rm -rf "$TMP_REPO_DIR"
  git clone --branch "$PRIVATE_BRANCH" --depth 1 "$PRIVATE_REPO" "$TMP_REPO_DIR"
  echo "$PRIVATE_REPO" > "$SAVED_PRIVATE_REPO_FILE"
  echo "$PRIVATE_BRANCH" > "$SAVED_PRIVATE_BRANCH_FILE"
  echo "Saved private repository url '$PRIVATE_REPO' to '$SAVED_PRIVATE_REPO_FILE'"
  echo "Saved private branch '$PRIVATE_BRANCH' to '$SAVED_PRIVATE_BRANCH_FILE'"
  cd $TMP_REPO_DIR
  rm -rf "$TMP_REPO_DIR/.git" "$TMP_REPO_DIR/README.md"
  cp -Rv "$TMP_REPO_DIR"/* "$BASE_PATH"
  rm -rf "$TMP_REPO_DIR"
  # Remove old android secrets during the transition period to the new project structure
  echo "Removing keys from old locations"
  rm -f android/release.keystore \
        android/secure.properties \
        android/libnotify.properties \
        android/google-services.json \
        android/google-play.json \
        android/firebase-app-distribution.json \
        android/firebase-test-lab.json \
        android/huawei-appgallery.json \
        android/res/xml/network_security_config.xml
  set +x
  echo "Private files have been updated."
}

if [ "${1-}" = "-h" -o "${1-}" = "--help" ]; then
  usage
  exit 1
fi

ARGS_PRIVATE_REPO=${1-}
ARGS_PRIVATE_BRANCH=${2-}

if [ -n "$ARGS_PRIVATE_REPO" ]; then
  PRIVATE_REPO=$ARGS_PRIVATE_REPO
  if [ -n "$ARGS_PRIVATE_BRANCH" ]; then
    PRIVATE_BRANCH=$ARGS_PRIVATE_BRANCH
  else
    PRIVATE_BRANCH=master
  fi
else
  read -t 1 READ_PRIVATE_REPO READ_PRIVATE_BRANCH || true
  if [ -n "${READ_PRIVATE_REPO-}" ]; then
    PRIVATE_REPO=$READ_PRIVATE_REPO
    if [ -n "${READ_PRIVATE_BRANCH-}" ]; then
      PRIVATE_BRANCH=$READ_PRIVATE_BRANCH
    else
      PRIVATE_BRANCH=master
    fi
  elif [ -f "$SAVED_PRIVATE_REPO_FILE" ]; then
    PRIVATE_REPO=`cat "$SAVED_PRIVATE_REPO_FILE"`
    echo "Using stored private repository URL: $PRIVATE_REPO"
    if [ -f "$SAVED_PRIVATE_BRANCH_FILE" ]; then
      PRIVATE_BRANCH=`cat "$SAVED_PRIVATE_BRANCH_FILE"`
      echo "Using stored private branch: $PRIVATE_BRANCH"
    else
      PRIVATE_BRANCH=master
    fi
  else
    PRIVATE_REPO=""
  fi
fi

if [ -n "$PRIVATE_REPO" ]; then
  setup_private
else
  setup_opensource
fi

# TODO: Remove these lines when XCode project is finally generated by CMake.
if [ ! -d "$BASE_PATH/3party/boost/" ]; then
  echo "You need to have Boost submodule present to run bootstrap.sh"
  echo "Try 'git submodule update --init --recursive'"
  exit 1
fi
if [ ! -d "$BASE_PATH/3party/boost/tools" ]; then
  echo "Boost's submodule 'tools' is missing, it is required for bootstrap"
  echo "Try 'git submodule update --init --recursive'"
  exit 1
fi

cd "$BASE_PATH/3party/boost/"
if [[ "$OSTYPE" == msys ]]; then
  echo "For Windows please run:"
  echo "cd 3party\\boost"
  echo "bootstrap.bat"
  echo "b2 headers"
  echo "cd ..\\.."
else
  ./bootstrap.sh
  ./b2 headers
fi
cd "$BASE_PATH"

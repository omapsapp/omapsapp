#!/usr/bin/env bash

set -eu

OPT_DEBUG=
OPT_RELEASE=
OPT_CLEAN=
OPT_DESIGNER=
OPT_GCC=
OPT_TARGET=
OPT_PATH=
OPT_STANDALONE=
OPT_COMPILE_DATABASE=
OPT_LAUNCH_BINARY=
OPT_NJOBS=
while getopts ":cdrxtagjlp:n:" opt; do
  case $opt in
    a) OPT_STANDALONE=1 ;;
    c) OPT_CLEAN=1 ;;
    d) OPT_DEBUG=1 ;;
    x) CMAKE_CONFIG="${CMAKE_CONFIG:-} -DUSE_PCH=YES" ;;
    g) OPT_GCC=1 ;;
    j) OPT_COMPILE_DATABASE=1
       CMAKE_CONFIG="${CMAKE_CONFIG:-} -DCMAKE_EXPORT_COMPILE_COMMANDS=YES"
       ;;
    l) OPT_LAUNCH_BINARY=1 ;;
    n) OPT_NJOBS="$OPTARG"
       CMAKE_CONFIG="${CMAKE_CONFIG:-} -DNJOBS=${OPT_NJOBS}"
       ;;
    p) OPT_PATH="$OPTARG" ;;
    r) OPT_RELEASE=1 ;;
    t) OPT_DESIGNER=1 ;;
    *)
      echo "This tool builds Organic Maps"
      echo "Usage: $0 [-d] [-r] [-c] [-x] [-s] [-t] [-a] [-g] [-j] [-l] [-p PATH] [-n NUM] [target1 target2 ...]"
      echo
      echo "By default both debug and release versions are built in ../omim-build-<buildtype> dir."
      echo
      echo -e "-d  Build a debug version"
      echo -e "-r  Build a release version"
      echo -e "-x  Use pre-compiled headers"
      echo -e "-c  Clean before building"
      echo -e "-t  Build Qt based designer tool (Linux/MacOS only)"
      echo -e "-a  Build Qt based standalone desktop app (Linux/MacOS only)"
      echo -e "-g  Force use GCC (Linux/MacOS only)"
      echo -e "-p  Directory for built binaries"
      echo -e "-n  Number of parallel processes"
      echo -e "-j  Generate compile_commands.json"
      echo -e "-l  Launches built binaries, useful for tests"
      exit 1
      ;;
  esac
done

OPT_TARGET=${@:$OPTIND}

NO_QT_LOCATION=
CMAKE_CONFIG="${CMAKE_CONFIG:-} -U SKIP_QT_GUI"
if [ "$OPT_TARGET" != "desktop" -a -z "$OPT_DESIGNER" -a -z "$OPT_STANDALONE" ]; then
  CMAKE_CONFIG="${CMAKE_CONFIG:-} -DSKIP_QT_GUI=ON"
  NO_QT_LOCATION=1
fi

# By default build everything
if [ -z "$OPT_DEBUG$OPT_RELEASE" ]; then
  OPT_DEBUG=1
  OPT_RELEASE=1
fi

OMIM_PATH="$(cd "${OMIM_PATH:-$(dirname "$0")/../..}"; pwd)"
if ! grep "DEFAULT_URLS_JSON" "$OMIM_PATH/private.h" >/dev/null 2>/dev/null; then
  echo "Please run $OMIM_PATH/configure.sh"
  exit 2
fi

DEVTOOLSET_PATH=/opt/rh/devtoolset-7
if [ -d "$DEVTOOLSET_PATH" ]; then
  export MANPATH=
  source "$DEVTOOLSET_PATH/enable"
else
  DEVTOOLSET_PATH=
fi

# Find cmake
source "$OMIM_PATH/tools/autobuild/detect_cmake.sh"

# OS-specific parameters
if [ "$(uname -s)" == "Darwin" ]; then
  PROCESSES=$(sysctl -n hw.ncpu)

  if [ -n "$OPT_GCC" ]; then
    GCC="$(ls /usr/local/bin | grep '^gcc-[6-9][0-9]\?' -m 1)" || true
    GPP="$(ls /usr/local/bin | grep '^g++-[6-9][0-9]\?' -m 1)" || true
    [ -z "$GCC" -o -z "$GPP" ] \
    && echo "Either GCC or G++ is not found. (The minimum supported GCC version is 6)." \
    && exit 2
    CMAKE_CONFIG="${CMAKE_CONFIG:-} -DCMAKE_C_COMPILER=/usr/local/bin/$GCC \
                                    -DCMAKE_CXX_COMPILER=/usr/local/bin/$GPP"
  fi
elif [ "$(uname -s)" == "Linux" ]; then
  PROCESSES=$(nproc)
else
  [ -n "$OPT_DESIGNER" ] \
  && echo "Designer tool is only supported on Linux or MacOS" && exit 2
  [ -n "$OPT_STANDALONE" ] \
  && echo "Standalone desktop app is only supported on Linux or MacOS" && exit 2
  PROCESSES=$(nproc)
fi

if [ -n "$OPT_NJOBS" ]; then
  PROCESSES="$OPT_NJOBS"
fi

build()
{
  local MAKE_COMMAND=$(which ninja)
  local CMAKE_GENERATOR=
  if [ -z "$MAKE_COMMAND" ]; then
    echo "Ninja is not found, using Make instead"
    MAKE_COMMAND="make -j $PROCESSES"
  else
    CMAKE_GENERATOR=-GNinja
  fi

  CONF=$1
  if [ -n "$OPT_PATH" ]; then
    DIRNAME="$OPT_PATH/omim-build-$(echo "$CONF" | tr '[:upper:]' '[:lower:]')"
  else
    DIRNAME="$OMIM_PATH/../omim-build-$(echo "$CONF" | tr '[:upper:]' '[:lower:]')"
  fi
  if [ -d "$DIRNAME" -a -n "$OPT_CLEAN" ]; then
    rm -r "$DIRNAME"
  else 
    if grep -q qt_location "$DIRNAME/platform/platform_autogen/mocs_compilation.cpp"; then
      if [ -n "${NO_QT_LOCATION}" ]; then
        # If mocs file includes the qt_location dependent unit but the target doesn't need it
        rm -f "$DIRNAME/platform/platform_autogen/mocs_compilation.cpp"
      fi
    else
      if [ -z "${NO_QT_LOCATION}" ]; then
        # If mocs file is missing the qt_location dependent unit but the target requires it
        rm -f "$DIRNAME/platform/platform_autogen/mocs_compilation.cpp"
      fi
    fi
  fi
  if [ ! -d "$DIRNAME" ]; then
    mkdir -p "$DIRNAME"
  fi
  cd "$DIRNAME"
  if [ -z "$OPT_DESIGNER" ]; then
    "$CMAKE" "$CMAKE_GENERATOR" "$OMIM_PATH" \
      -DCMAKE_BUILD_TYPE="$CONF" \
      -DBUILD_DESIGNER:BOOL=OFF \
      -DBUILD_STANDALONE:BOOL=$([ "$OPT_STANDALONE" == 1 ] && echo "ON" || echo "OFF") \
      ${CMAKE_CONFIG:-}
    echo ""
    $MAKE_COMMAND $OPT_TARGET
    if [ -n "$OPT_TARGET" ] && [ -n "$OPT_LAUNCH_BINARY" ]; then
      for target in $OPT_TARGET; do
        "$DIRNAME/$target"
      done
    fi
  else
    "$CMAKE" "$CMAKE_GENERATOR" "$OMIM_PATH" -DCMAKE_BUILD_TYPE="$CONF" -DBUILD_DESIGNER:BOOL=ON ${CMAKE_CONFIG:-}
    $MAKE_COMMAND package
  fi
  if [ -n "$OPT_COMPILE_DATABASE" ]; then
    cp "$DIRNAME/compile_commands.json" "$OMIM_PATH"
  fi
}

[ -n "$OPT_DEBUG" ]   && build Debug
[ -n "$OPT_RELEASE" ] && build Release
exit 0

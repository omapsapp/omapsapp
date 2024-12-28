#!/bin/bash
# Should be used everywhere to generate a consistent version number based
# on the date of the last commit and a number of commits on that day.
set -euo pipefail


function init {
  if ! type git >/dev/null 2>&1 || ! git rev-parse --is-inside-work-tree >/dev/null 2>&1 ; then
    # Either git is not installed on the system or this is not a git work tree.
    # Likely because the code is built from an archive.
    # In any case determining commit count, date and hash won't be possible.
    local COUNT_AND_DATE=( 0 $(date +%Y.%m.%d) )
    GIT_HASH="00000000"
  else
    # Note: other ways to get date use the "when commit was rebased" date.
    # This approach counts a number of commits each day based on committer's commit date
    # instead of author's commit date, to avoid conflicts when old PRs are merged, but the
    # number of a day's commits stays the same.
    # Force git with TZ variable and local dates to print the UTC date.
    local COUNT_AND_DATE=( $(TZ=UTC0 git log --max-count=128 --pretty=format:%cd --date=iso-local \
                                       | cut -d' ' -f 1 | sed 's/-/./g' | sort | uniq -c | tail -1) )
    GIT_HASH=$(git describe --match="" --always --abbrev=8 --dirty)
  fi

  DATE="${COUNT_AND_DATE[1]}"
  COUNT="${COUNT_AND_DATE[0]}"
}

function ios_version {
  echo "$DATE"
}

function ios_build {
  MINOR=$((16#${GIT_HASH:0:4}))
  PATCH=$((16#${GIT_HASH:4:4}))
  echo "$COUNT.$MINOR.$PATCH"
}

function  android_name {
  echo "$DATE-$COUNT"
}

function android_code {
  # RR_yy_MM_dd_CC
  # RR - reserved to identify special markets, max value is 21.
  # yy - year
  # MM - month
  # dd - day
  # CC - the number of commits from the current day
  # 21_00_00_00_00 is the the greatest value Google Play allows for versionCode.
  # See https://developer.android.com/studio/publish/versioning for details.
  local cutYear=${DATE:2}
  echo "${cutYear//./}$(printf %02d "$COUNT")"
}

function qt_int_version {
  # yy_MM_dd
  # yy - year
  # MM - month
  # dd - day
  local cutYear=${DATE:2}
  echo "${cutYear//./}"
}

function qt_version {
  local OS_NAME=$(uname -s)
  echo "$DATE-$COUNT-$GIT_HASH-$OS_NAME"
}

function usage {
  cat << EOF
Prints Organic Maps version in specified format.
Version is the last git commit's date plus a number of commits on that day.
Usage: $0 <format>
Where format is one of the following arguments (shows current values):
  ios_version    $(ios_version)
  ios_build      $(ios_build)
  android_name   $(android_name)
  android_code   $(android_code)
  qt_version     $(qt_version)
  qt_int_version $(qt_int_version)
EOF
}

init

if [ -z ${1:-} ] || [[ ! $(type -t "$1") == function ]]; then
  usage
  exit 1
else
  "$1"
fi

#!/bin/bash
set -euxo pipefail

# Setup is copied from generate_localizations.sh

# Use ruby from brew on Mac OS X, because system ruby is outdated/broken/will be removed in future releases.
case $OSTYPE in
  darwin*)
    if [ -x /usr/local/opt/ruby/bin/ruby ]; then
      PATH="/usr/local/opt/ruby/bin:$PATH"
    elif [ -x "${HOMEBREW_PREFIX:-/opt/homebrew}/opt/ruby/bin/ruby" ]; then
      PATH="${HOMEBREW_PREFIX:-/opt/homebrew}/opt/ruby/bin:$PATH"
    else
      echo 'Please install Homebrew ruby by running "brew install ruby"'
      exit 1
    fi ;;
  *)
    if [ ! -x "$(which ruby)" ]; then
      echo "Please, install ruby (https://www.ruby-lang.org/en/documentation/installation/)"
      exit 1
    fi ;;
esac

THIS_SCRIPT_PATH=$(cd "$(dirname "$0")"; pwd -P)
OMIM_PATH="$THIS_SCRIPT_PATH/../.."
TWINE_PATH="$OMIM_PATH/tools/twine"

if [ ! -e "$TWINE_PATH/twine" ]; then
  echo "You need to have twine submodule present to run this script"
  echo "Try 'git submodule update --init --recursive'"
  exit 1
fi

TWINE_COMMIT="$(git -C $TWINE_PATH rev-parse HEAD)"
TWINE_GEM="twine-$TWINE_COMMIT.gem"

if [ ! -f "$TWINE_PATH/$TWINE_GEM" ]; then
  echo "Building & installing twine gem..."
  (
    cd "$TWINE_PATH" \
    && rm -f ./*.gem \
    && gem build --output "$TWINE_GEM" \
    && gem install --user-install "$TWINE_GEM"
  )
fi

# Generate android/iphone/jquery localization files from strings files.
TWINE="$(gem contents --show-install-dir twine)/bin/twine"
if [[ $TWINE == *".om/bin/twine" ]]; then
  echo "Using the correctly patched submodule version of Twine"
else
  echo "Looks like you have a non-patched version of twine, try to uninstall it with '[sudo] gem uninstall twine'"
  exit 1
fi

STRINGS_PATH="$OMIM_PATH/data/strings"

MERGED_FILE="$(mktemp)"
cat "$STRINGS_PATH"/{strings,types_strings}.txt> "$MERGED_FILE"

#"$TWINE" consume-all-localization-files -h
"$TWINE" consume-all-localization-files "$STRINGS_PATH"/strings.txt "$OMIM_PATH/android/app/src/main/res/" --format android --quiet --developer-language en
"$TWINE" consume-all-localization-files "$STRINGS_PATH"/types_strings.txt "$OMIM_PATH/android/app/src/main/res/" --format android --developer-language en
sed -i "" -E '/zh-MO/d' "$STRINGS_PATH"/strings.txt "$STRINGS_PATH"/types_strings.txt
sed -i "" -E '/zh-TW/d' "$STRINGS_PATH"/strings.txt "$STRINGS_PATH"/types_strings.txt

#!/bin/bash
set -euxo pipefail

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

OMIM_PATH="$(dirname "$0")/../.."
TWINE_SUBMODULE=tools/twine
TWINE_PATH="$OMIM_PATH/$TWINE_SUBMODULE"

if [ ! -e "$TWINE_PATH/twine" ]; then
  echo "You need to have twine submodule present to run this script"
  echo "Try 'git submodule update --init --recursive'"
  exit 1
fi

TWINE_COMMIT="$(git -C $TWINE_SUBMODULE rev-parse HEAD)"
TWINE_GEM="twine-$TWINE_COMMIT.gem"

if [ ! -f "$TWINE_PATH/$TWINE_GEM" ] || ! gem list -i twine; then
  echo "Building & installing twine gem..."
  (
    cd "$TWINE_PATH" \
    && rm -f ./*.gem \
    && gem build --output "$TWINE_GEM" \
    && gem install --user-install "$TWINE_GEM"
  )
fi

# Validate and format/sort strings files.
STRINGS_UTILS="$OMIM_PATH/tools/python/strings_utils.py"
"$STRINGS_UTILS" --validate --output
"$STRINGS_UTILS" --types-strings --validate --output

# Check for unused strings.
CLEAN_STRINGS="$OMIM_PATH/tools/python/clean_strings_txt.py"
"$CLEAN_STRINGS" --validate

# Generate android/iphone/jquery localization files from strings files.
TWINE="$(gem contents twine | grep -m 1 bin/twine)"
STRINGS_PATH="$OMIM_PATH/data/strings"

MERGED_FILE="$(mktemp)"
cat "$STRINGS_PATH"/{strings,types_strings}.txt> "$MERGED_FILE"

# TODO: Add validate-strings-file call to check for duplicates (and avoid Android build errors) when tags are properly set.
"$TWINE" generate-all-localization-files --include translated --format android --untagged --tags android "$MERGED_FILE" "$OMIM_PATH/android/res/"
"$TWINE" generate-all-localization-files --format apple --untagged --tags ios "$MERGED_FILE" "$OMIM_PATH/iphone/Maps/LocalizedStrings/"
"$TWINE" generate-all-localization-files --format apple-plural --untagged --tags ios "$MERGED_FILE" "$OMIM_PATH/iphone/Maps/LocalizedStrings/"
"$TWINE" generate-all-localization-files --format apple --file-name InfoPlist.strings "$OMIM_PATH/iphone/plist.txt" "$OMIM_PATH/iphone/Maps/LocalizedStrings/"
"$TWINE" generate-all-localization-files --format jquery "$OMIM_PATH/data/countries_names.txt" "$OMIM_PATH/data/countries-strings/"
"$TWINE" generate-all-localization-files --format jquery "$OMIM_PATH/data/strings/sound.txt" "$OMIM_PATH/data/sound-strings/"

rm "$MERGED_FILE"

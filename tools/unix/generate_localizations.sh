#!/bin/bash
set -euo pipefail

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

OMIM_DATA="$OMIM_PATH/data"
STRINGS_PATH="$OMIM_DATA/strings"

# Validate and format/sort strings files.
STRINGS_UTILS="$OMIM_PATH/tools/python/strings_utils.py"
"$STRINGS_UTILS" --validate --output
"$STRINGS_UTILS" --types-strings --validate --output
"$STRINGS_UTILS" "$STRINGS_PATH/sound.txt" --validate --output
"$STRINGS_UTILS" "$OMIM_DATA/countries_names.txt" --validate --output
"$STRINGS_UTILS" "$OMIM_PATH/iphone/plist.txt" --validate --output

# Check for unused strings.
CLEAN_STRINGS="$OMIM_PATH/tools/python/clean_strings_txt.py"
"$CLEAN_STRINGS" --validate

MERGED_FILE="$(mktemp)"
cat "$STRINGS_PATH"/{strings,types_strings}.txt> "$MERGED_FILE"

# TODO: Add validate-strings-file call to check for duplicates (and avoid Android build errors) when tags are properly set.
"$TWINE" generate-all-localization-files --include translated --format android --untagged --tags android "$MERGED_FILE" "$OMIM_PATH/android/app/src/main/res/"
"$TWINE" generate-all-localization-files --format apple --untagged --tags ios "$MERGED_FILE" "$OMIM_PATH/iphone/Maps/LocalizedStrings/"
"$TWINE" generate-all-localization-files --format apple-plural --untagged --tags ios "$MERGED_FILE" "$OMIM_PATH/iphone/Maps/LocalizedStrings/"
"$TWINE" generate-all-localization-files --format apple --file-name InfoPlist.strings "$OMIM_PATH/iphone/plist.txt" "$OMIM_PATH/iphone/Maps/LocalizedStrings/"
"$TWINE" generate-all-localization-files --format jquery "$OMIM_DATA/countries_names.txt" "$OMIM_DATA/countries-strings/"
"$TWINE" generate-all-localization-files --format jquery "$STRINGS_PATH/sound.txt" "$OMIM_DATA/sound-strings/"

rm "$MERGED_FILE"

# Generate list of languages and add list in gradle.properties to be used in build.gradle in resConfig
SUPPORTED_LOCALIZATIONS="supportedLocalizations="$(sed -nEe "s/ +([a-zA-Z]{2}(-[a-zA-Z]{2,})?) = .*$/\1/p" "$STRINGS_PATH/strings.txt" | sort -u | tr '\n' ',' | sed -e 's/-/_/g' -e 's/,$//')
# Chinese locales should correspond to Android codes.
SUPPORTED_LOCALIZATIONS=${SUPPORTED_LOCALIZATIONS/zh_Hans/zh}
SUPPORTED_LOCALIZATIONS=${SUPPORTED_LOCALIZATIONS/zh_Hant/zh_HK,zh_MO,zh_TW}
SUPPORTED_LOCALIZATIONS=${SUPPORTED_LOCALIZATIONS/he/iw}
SUPPORTED_LOCALIZATIONS=${SUPPORTED_LOCALIZATIONS/id/in}
GRADLE_PROPERTIES="$OMIM_PATH/android/gradle.properties"
if [ "$SUPPORTED_LOCALIZATIONS" != "$(grep supportedLocalizations "$GRADLE_PROPERTIES")" ]; then
  sed -i.bak 's/supportedLocalizations.*/'"$SUPPORTED_LOCALIZATIONS"'/' "$GRADLE_PROPERTIES"
  rm "$GRADLE_PROPERTIES.bak"
fi

# Generate locales_config.xml to allow users change app's language on Android 13+
LOCALES_CONFIG="$OMIM_PATH/android/app/src/main/res/xml/locales_config.xml"
SUPPORTED_LOCALIZATIONS=${SUPPORTED_LOCALIZATIONS/supportedLocalizations=/en,}
SUPPORTED_LOCALIZATIONS=${SUPPORTED_LOCALIZATIONS/,en,/,}
SUPPORTED_LOCALIZATIONS=${SUPPORTED_LOCALIZATIONS//_/-}
LOCALES_CONTENT='<?xml version="1.0" encoding="utf-8"?>
<locale-config xmlns:android="http://schemas.android.com/apk/res/android">'
set +x
for lang in ${SUPPORTED_LOCALIZATIONS//,/ }; do
  LOCALES_CONTENT="$LOCALES_CONTENT"$'\n'"    <locale android:name=\"$lang\" />"
done
LOCALES_CONTENT="$LOCALES_CONTENT"$'\n''</locale-config>'
if [ "$LOCALES_CONTENT" != "$(cat "$LOCALES_CONFIG")" ]; then
  echo "$LOCALES_CONTENT" > "$LOCALES_CONFIG"
  echo Updated "$LOCALES_CONFIG" file
fi

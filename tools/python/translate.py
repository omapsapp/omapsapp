#!/usr/bin/env python3

# Requires `brew install translate-shell`
# and DEEPL_FREE_API_KEY or DEEPL_API_KEY environment variables set.

import os
import subprocess
import platform
import requests
import shutil
import sys

TRANS_CMD = 'trans'

# By default, most languages use formal translations.
INFORMAL_LANGUAGES = ['pt-BR']

# Use DeepL when possible with a fall back to Google.
# List of Google Translate target languages: https://cloud.google.com/translate/docs/languages
GOOGLE_TARGET_LANGUAGES = [
  'af',
  'az',
  'be',
  'ca',
  'eu',
  'fa',
  'he',
  'hi',
  'mr',
  'sw',
  'th',
  'vi',
  'zh-TW',  # zh-Hant in OM

  # Codes below duplicate the relevant ones in DeepL to have a fall-back when DeepL doesn't translate the word.
  'ar',
  'bg',
  'cs',
  'da',
  'de',
  'el',
#  'en-GB', # Not used in Google
#  'en-US', # en in OM, not used in Google
  'es',
  'et',
  'fi',
  'fr',
  'hu',
  'id',
  'it',
  'ja',
  'ko',
  'lt',
  'lv',
  'nb',
  'nl',
  'pl',
  'pt-BR', # Google does not support pt-PT
#  'pt-PT', # pt in OM
  'ro',
  'ru',
  'sk',
#  'sl',
  'sv',
  'tr',
  'uk',
  'zh-CN',  # zh-Hans in OM
]

# See https://developers.deepl.com/docs/resources/supported-languages#target-languages for target languages.
DEEPL_TARGET_LANGUAGES = [
  'ar',
  'bg',
  'cs',
  'da',
  'de',
  'el',
  'en-GB',
  'en-US', # en in OM
  'es',
  'et',
  'fi',
  'fr',
  'hu',
  'id',
  'it',
  'ja',
  'ko',
  'lt',
  'lv',
  'nb',
  'nl',
  'pl',
  'pt-BR',
  'pt-PT', # pt in OM
  'ro',
  'ru',
  'sk',
#  'sl',
  'sv',
  'tr',
  'uk',
  'zh',  # zh-Hans in OM
]
GOOGLE_TARGET_LANGUAGES.sort()

def get_api_key():
  key = os.environ.get('DEEPL_FREE_API_KEY')
  if key == None:
    key = os.environ.get('DEEPL_API_KEY')
  if key == None:
    print('Error: DEEPL_FREE_API_KEY or DEEPL_API_KEY env variables are not set')
    exit(1)
  return key

def google_translate(text, source_language):
  fromTo = source_language.lower() + ':'
  # Translate all languages with Google to replace failed DeepL translations.
  for lang in GOOGLE_TARGET_LANGUAGES:
    fromTo += lang + '+'
  # Remove last +
  fromTo = fromTo[:-1]
  res = subprocess.run([TRANS_CMD, '-b', '-no-bidi', fromTo, text], text=True, capture_output=True)
  if res.returncode != 0:
    print(f'Error running {TRANS_CMD} program:')
    print(res.stderr)
    exit(1)

  print('\nGoogle translations:')
  translations = {}
  i = 0
  for line in res.stdout.splitlines():
    lang = GOOGLE_TARGET_LANGUAGES[i]
    if lang == 'zh-TW':
      lang = 'zh-Hant'
    elif lang == 'pt-PT':
      lang = 'pt'
    elif lang == 'zh-CN' or lang == 'zh':
      lang = 'zh-Hans'
    translations[lang] = line
    i = i + 1
    print(lang + ' = ' + line)
  return translations

def google_translate_one(text, source_language, target_language):
  fromTo = source_language.lower() + ':' + target_language.lower()
  res = subprocess.run([TRANS_CMD, '-b', '-no-bidi', fromTo, text], text=True, capture_output=True)
  if res.returncode != 0:
    print(f'Error running {TRANS_CMD} program:')
    print(res.stderr)
    exit(1)
  return res.stdout.splitlines()[0]

def deepl_translate_one(text, source_language, target_language):
  url = 'https://api-free.deepl.com/v2/translate'
  payload = {
      'auth_key': get_api_key(),
      'text': text,
      'source_lang': source_language.lower(),
      'target_lang': target_language,
      'formality': 'prefer_less' if target_language in INFORMAL_LANGUAGES else 'prefer_more',
  }
  headers = {'Content-Type': 'application/x-www-form-urlencoded'}
  response = requests.request('POST', url, headers=headers, data=payload)
  json = response.json()
  return json['translations'][0]['text']

def translate_one(text, source_language, target_language):
  if target_language in DEEPL_TARGET_LANGUAGES:
    return deepl_translate_one(text, source_language, target_language)
  elif target_language in GOOGLE_TARGET_LANGUAGES:
    return google_translate_one(text, source_language, target_language)
  else:
    raise ValueError(f'Unsupported target language {target_language}')

def deepl_translate(text, source_language):
  translations = {}
  print('Deepl translations:')
  for lang in DEEPL_TARGET_LANGUAGES:
    translation = deepl_translate_one(text, source_language, lang)
    if lang == 'pt-PT':
      lang = 'pt'
    elif lang == 'zh':
      lang = 'zh-Hans'
    elif lang == 'en-US':
      lang = 'en'
    translations[lang] = translation
    print(lang + ' = ' + translation)
  return translations

def usage():
  print('Usage:', sys.argv[0], 'Some English text to translate')
  print('For a custom source language add a two-letter code with a colon in the beginning:')
  print('      ', sys.argv[0], 'de:Some German text to translate')
  print()
  print('Supported DeepL languages:')
  print(', '.join(DEEPL_TARGET_LANGUAGES))
  print('Supported Google languages:')
  print(', '.join(GOOGLE_TARGET_LANGUAGES))


# Returns a list of all languages supported by the core (search) in data/categories.txt
def get_supported_categories_txt_languages():
  script_dir = os.path.dirname(os.path.realpath(__file__))
  categories_txt_path = os.path.join(script_dir, '..', '..', 'data', 'categories.txt')
  languages = set()
  with open(categories_txt_path) as f:
    for line in f.readlines():
      if not line: continue
      if line[0] == '#': continue
      colon_index = line.find(':')
      # en: en-US: zh-Hant:
      if colon_index == 2 or colon_index == 5 or colon_index == 7:
        languages.add(line[:colon_index])

  # Convert to list.
  languages = list(languages)
  languages.sort()
  return languages

if __name__ == '__main__':
  if len(sys.argv) < 2:
    usage()
    exit(1)

  if not 'DEEPL_FREE_API_KEY' in os.environ and not 'DEEPL_API_KEY' in os.environ:
    print('Error: neither DEEPL_FREE_API_KEY nor DEEPL_API_KEY environment variables are set.')
    print('DeepL translations are not available. Register for a free Developer API account here:')
    print('https://www.deepl.com/pro#developer')
    print('and get the API key here: https://www.deepl.com/account/summary')
    exit(1)

  if shutil.which(TRANS_CMD) == None:
    print('Error: translate-shell program for Google Translate is not installed.')
    if platform.system() == 'Darwin':
        print('Install it using `brew install translate-shell`')
    else:
        print('See https://github.com/soimort/translate-shell/wiki/Distros for installation instructions.')
    exit(1)

  text_to_translate = ' '.join(sys.argv[1:])

  source_language = 'en'
  if len(text_to_translate) > 3 and text_to_translate[2] == ':':
    source_language = text_to_translate[0:2]
    text_to_translate = text_to_translate[3:].lstrip()

  translations = deepl_translate(text_to_translate, source_language)
  google_translations = google_translate(text_to_translate, source_language)
  # Check if DeepL did not translate the string (it is the same as the original string),
  # and fall back to Google in this case.
  original_input = translations[source_language]
  for lang, value in google_translations.items():
    if lang in translations:
      if translations[lang] == original_input:
        translations[lang] = value
    else:
      translations[lang] = value

  # Remove duplicates for regional variations.
  for regional in ['en-GB', 'pt-BR']:
    main = regional.split('-')[0]  # 'en', 'pt', ...
    if translations[regional] == translations[main]:
      translations.pop(regional)

  print('\nMerged Deepl and Google translations:')
  en = translations.pop('en')
  langs = list(translations.keys())
  langs.sort()

  categories_txt_languages = get_supported_categories_txt_languages()
  absent_in_categories_txt = [item for item in langs if item not in categories_txt_languages]
  print('============ categories.txt format ============')
  if len(absent_in_categories_txt) > 0:
    print('\nWARNING: The following translations are not supported yet in the categories.txt and are skipped:')
    print(absent_in_categories_txt)
    print('See indexer/categories_holder.hpp for details.\n')
  print('en:' + en)
  for lang in langs:
    if lang in absent_in_categories_txt:
      continue
    print(lang + ':' + translations[lang])

  print('============ strings.txt format ============')
  print('    en =', en)
  for lang in langs:
    print('   ', lang, '=', translations[lang])

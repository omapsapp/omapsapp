#!/usr/bin/env python3

"""
This script checks for consistency between categories defined in:
1. `displayed_categories.cpp`: A C++ source file containing category keys.
2. `categories.txt`: A data file defining category details.
3. `strings.txt`: A data file containing translations of category strings.

The script:
- Parses category keys from the C++ file.
- Loads corresponding data from `categories.txt` and `strings.txt`.
- Compares the parsed data for inconsistencies in definitions and translations.
- Prints detailed messages if inconsistencies are found.

It exits with:
- `0` on success (all categories are consistent),
- `1` on failure (inconsistencies detected).
"""

import os
import re
from omim_parsers import CategoriesParser, StringsParser, LANGUAGES

ROOT = os.path.dirname(os.path.abspath(__file__))
OMIM_ROOT = os.path.join(ROOT, '..', '..', '..')
CPP_CATEGORIES_FILENAME = os.path.join(OMIM_ROOT, 'search', 'displayed_categories.cpp')
CATEGORIES_FILENAME = os.path.join(OMIM_ROOT, 'data', 'categories.txt')
STRINGS_FILENAME = os.path.join(OMIM_ROOT, 'data', 'strings', 'strings.txt')
CATEGORIES_MATCHER = re.compile(r"m_keys = \{(.*?)};", re.DOTALL)


def extract_cpp_categories(filename):
    if not os.path.exists(filename):
        print(f"Error: {filename} not found.")
        return []

    with open(filename, "r", encoding="utf-8") as cpp_file:
        content = cpp_file.read()

    match = CATEGORIES_MATCHER.search(content)
    if not match:
        print(f"Error: No categories found in {filename}.")
        return []

    raw_categories = match.group(1)
    return [cat.strip().strip('"') for cat in raw_categories.split(",")]


def compare_categories(string_cats, search_cats, cpp_cats):
    inconsistent_strings = {}
    missing_categories = []
    extra_categories = []

    for category_name in search_cats.keys():
        if category_name not in string_cats:
            missing_categories.append(category_name)

    for cpp_cat in cpp_cats:
        if cpp_cat not in search_cats:
            extra_categories.append(cpp_cat)

    for category_name, translations in search_cats.items():
        if category_name not in string_cats:
            continue

        for lang, search_translation in translations.items():
            if lang not in string_cats[category_name]:
                inconsistent_strings.setdefault(category_name, {})[lang] = (
                    "Missing translation",
                    search_translation,
                )
            elif string_cats[category_name][lang] != search_translation:
                inconsistent_strings.setdefault(category_name, {})[lang] = (
                    string_cats[category_name][lang],
                    search_translation,
                )

    if missing_categories:
        print("\nMissing translations for categories in strings.txt:")
        for category_name in missing_categories:
            print(f"  - {category_name}")

    if extra_categories:
        print("\nExtra categories found in displayed_categories.cpp but not in categories.txt:")
        for cpp_cat in extra_categories:
            print(f"  - {cpp_cat}")

    if inconsistent_strings:
        print("\nInconsistent category translations:")
        for category_name, langs in inconsistent_strings.items():
            print(f"Category \"{category_name}\":")
            for lang, (strings_value, search_value) in langs.items():
                print(f"  {lang}: strings.txt=\"{strings_value}\" vs categories.txt=\"{search_value}\"")

    return not (missing_categories or extra_categories or inconsistent_strings)


def check_search_categories_consistent():
    categories_txt_parser = CategoriesParser(LANGUAGES)
    strings_txt_parser = StringsParser(LANGUAGES)

    if not os.path.exists(CATEGORIES_FILENAME):
        print(f"Error: {CATEGORIES_FILENAME} not found.")
        return 1

    if not os.path.exists(STRINGS_FILENAME):
        print(f"Error: {STRINGS_FILENAME} not found.")
        return 1

    search_categories = categories_txt_parser.parse_file(CATEGORIES_FILENAME)
    string_categories = strings_txt_parser.parse_file(STRINGS_FILENAME)
    cpp_categories = extract_cpp_categories(CPP_CATEGORIES_FILENAME)

    if compare_categories(string_categories, search_categories, cpp_categories):
        print("Success: All categories are consistent.")
        return 0
    else:
        print("Failure: Inconsistencies found in category definitions.")
        return 1


if __name__ == "__main__":
    exit(check_search_categories_consistent())

#!/usr/bin/env python3

import logging
import re
import subprocess
from argparse import ArgumentParser
from collections import defaultdict
from itertools import chain
from os.path import abspath, isabs

from strings_utils import StringsTxt

"""
This script determines which strings are used in the platform code (iOS and
Android) and removes all the other strings from strings.txt. For more information,
run this script with the -h option.
"""


OMIM_ROOT = ""

CORE_RE = re.compile(r'GetLocalizedString\("(.*?)"\)')

# max 2 matches in L(). Tried to make ()+ group, but no luck ..
IOS_RE = re.compile(r'L\(.*?"(\w+)".*?(?:"(\w+)")?\)')
IOS_NS_RE = re.compile(r'NSLocalizedString\(\s*?@?"(\w+)"')
IOS_XML_RE = re.compile(r'value=\"(.*?)\"')
IOS_APPTIPS_RE = re.compile(r'app_tip_\d\d')

ANDROID_JAVA_RE = re.compile(r'R\.string\.([\w_]*)')
ANDROID_JAVA_PLURAL_RE = re.compile(r'R\.plurals\.([\w_]*)')
ANDROID_XML_RE = re.compile(r'@string/(.*?)\W')

IOS_CANDIDATES_RE = re.compile(r'(.*?):[^L\(]@"([a-z0-9_]*?)"')

HARDCODED_CATEGORIES = []

HARDCODED_STRINGS = [
    # titleForBookmarkColor
    "red", "blue", "purple", "yellow", "pink", "brown", "green", "orange", "deep_purple", "light_blue",
    "cyan", "teal", "lime", "deep_orange", "gray", "blue_gray",
]


def exec_shell(test, *flags):
    spell = ["{0} {1}".format(test, list(*flags))]

    process = subprocess.Popen(
        spell,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        shell=True
    )

    logging.info(" ".join(spell))
    out, _ = process.communicate()
    return [line for line in out.decode().splitlines() if line]


def grep_ios():
    logging.info("Grepping iOS...")
    grep = "grep -r -I 'L(\\|localizedText\\|localizedPlaceholder\\|NSLocalizedString(' {0}/iphone/*".format(
        OMIM_ROOT)
    ret = exec_shell(grep)
    ret = filter_ios_grep(ret)
    logging.info("Found in iOS: {0}".format(len(ret)))
    ret.update(get_hardcoded())

    # iOS code scans resources for all available app_tip_XX strings.
    grep = "grep app_tip_ {0}/data/strings/strings.txt".format(OMIM_ROOT)
    ret2 = exec_shell(grep)
    ret.update(parenthesize(strings_from_grepped(ret2, IOS_APPTIPS_RE)))

    return ret


def grep_android():
    logging.info("Grepping android...")
    grep = "grep -r -I 'R.string.' {0}/android/app/src/main".format(OMIM_ROOT)
    ret = android_grep_wrapper(grep, ANDROID_JAVA_RE)
    grep = "grep -r -I 'R.plurals.' {0}/android/app/src/main".format(OMIM_ROOT)
    ret.update(android_grep_wrapper(grep, ANDROID_JAVA_PLURAL_RE))
    grep = "grep -r -I '@string/' {0}/android/app/src/main/res".format(OMIM_ROOT)
    ret.update(android_grep_wrapper(grep, ANDROID_XML_RE))
    grep = "grep -r -I '@string/' {0}/android/app/src/google/res".format(OMIM_ROOT)
    ret.update(android_grep_wrapper(grep, ANDROID_XML_RE))
    grep = "grep -r -I '@string/' {0}/android/app/src/main/AndroidManifest.xml".format(
        OMIM_ROOT)
    ret.update(android_grep_wrapper(grep, ANDROID_XML_RE))
    ret = parenthesize(ret)

    logging.info("Found in android: {0}".format(len(ret)))
    ret.update(get_hardcoded())

    return ret


def grep_core():
    logging.info("Grepping core...")
    grep = "grep -wr -I 'GetLocalizedString' {0}/map {0}/platform".format(OMIM_ROOT)
    ret = android_grep_wrapper(grep, CORE_RE)
    logging.info("Found in core: {0}".format(len(ret)))

    return parenthesize(ret)


def grep_ios_candidates():
    logging.info("Grepping iOS candidates...")
    grep = "grep -nr -I '@\"' {0}/iphone/*".format(OMIM_ROOT)
    ret = exec_shell(grep)
    logging.info("Found in iOS candidates: {0}".format(len(ret)))

    strs = strings_from_grepped(ret, IOS_CANDIDATES_RE)
    return strs


def get_hardcoded():
    "search/displayed_categories.cpp"
    ret = parenthesize(HARDCODED_CATEGORIES)
    ret.update(parenthesize(HARDCODED_STRINGS))
    logging.info("Hardcoded colors and categories: {0}".format(len(ret)))
    return ret


def android_grep_wrapper(grep, regex):
    grepped = exec_shell(grep)
    return strings_from_grepped(grepped, regex)


def filter_ios_grep(strings):
    filtered = strings_from_grepped_tuple(strings, IOS_RE)
    filtered = parenthesize(process_ternary_operators(filtered))
    filtered.update(parenthesize(strings_from_grepped(strings, IOS_NS_RE)))
    filtered.update(parenthesize(strings_from_grepped(strings, IOS_XML_RE)))
    return filtered


def process_ternary_operators(filtered):
    return chain(*(s.split('" : @"') for s in filtered))


def strings_from_grepped(grepped, regexp):
    return set(chain(*(regexp.findall(s) for s in grepped if s)))


def strings_from_grepped_tuple(grepped, regexp):
    res = set()
    for e1 in grepped:
        for e2 in regexp.findall(e1):
            for e3 in e2:
                if e3:
                    res.add(e3)
    return res


def parenthesize(strings):
    return set("[{}]".format(s) for s in strings)


def write_filtered_strings_txt(filtered, filepath, languages=None):
    logging.info("Writing strings to file {0}".format(filepath))
    strings_txt = StringsTxt(
        "{0}/{1}".format(OMIM_ROOT, StringsTxt.STRINGS_TXT_PATH))
    strings_dict = {
        key: dict(strings_txt.translations[key]) for key in filtered}
    strings_txt.translations = strings_dict
    strings_txt.comments_tags_refs = {}
    strings_txt.write_formatted(target_file=filepath, langs=languages)


def get_args():
    parser = ArgumentParser(
        description="""
        A script for cleaning up the strings.txt file. It can cleanup the file
        inplace, that is all the unused strings will be removed from strings.txt,
        or it can produce two separate files for ios and android. We can also
        produce the compiled string resources specifically for each platform
        that do not contain strings for other platforms or comments."""
    )

    parser.add_argument(
        "-v", "--validate",
        dest="validate",
        action="store_true",
        help="""Check for translation definitions/keys which are no longer
        used in the codebase, exit with error if found"""
    )

    parser.add_argument(
        "-s", "--single-file",
        dest="single",
        action="store_true",
        help="""Create single cleaned up file for both platform. All strings
        that are not used in the project will be thrown away. Otherwise, two
        platform specific files will be produced."""
    )

    parser.add_argument(
        "-l", "--language",
        dest="langs", default=None,
        action="append",
        help="""The languages to be included into the resulting strings.txt
        file or files. If this param is empty, all languages from the current
        strings.txt file will be preserved."""
    )

    parser.add_argument(
        "-g", "--generate-localizations",
        dest="generate",
        action="store_true",
        help="Generate localizations for the platforms."
    )

    parser.add_argument(
        "-o", "--output",
        dest="output", default="data/strings/strings.txt",
        help="""The name for the resulting file. It will be saved to the
        project folder. Only relevant if the -s option is set."""
    )

    parser.add_argument(
        "-m", "--missing-strings",
        dest="missing",
        action="store_true",
        help="""Find the keys that are used in iOS, but are not translated
        in strings.txt and exit."""
    )

    parser.add_argument(
        "-c", "--candidates",
        dest="candidates",
        action="store_true",
        help="""Find the strings in iOS that are not in the L() macros, but that
        look like they might be keys."""
    )

    parser.add_argument(
        "-r", "--root",
        dest="omim_root", default=find_omim(),
        help="Path to the root of the OMIM project"
    )

    return parser.prog, parser.parse_args()


def do_multiple(args):
    write_filtered_strings_txt(
        grep_ios(), "{0}/ios_strings.txt".format(OMIM_ROOT), args.langs
    )
    write_filtered_strings_txt(
        grep_android(), "{0}/android_strings.txt".format(OMIM_ROOT), args.langs
    )
    if args.generate:
        print("Going to generate locs")
        exec_shell(
            "{0}/tools/unix/generate_localizations.sh".format(OMIM_ROOT),
            "android_strings.txt", "ios_strings.txt"
        )


def generate_auto_tags(ios, android, core):
    new_tags = defaultdict(set)
    for i in ios:
        new_tags[i].add("ios")

    for a in android:
        new_tags[a].add("android")

    for c in core:
        new_tags[c].add("ios")
        new_tags[c].add("android")

    return new_tags


def new_comments_and_tags(strings_txt, filtered, new_tags):
    comments_and_tags = {
        key: strings_txt.comments_tags_refs[key] for key in filtered}
    for key in comments_and_tags:
        comments_and_tags[key]["tags"] = ",".join(sorted(new_tags[key]))
    return comments_and_tags


def do_single(args):
    core = grep_core()
    ios = grep_ios()
    android = grep_android()

    new_tags = generate_auto_tags(ios, android, core)

    filtered = ios
    filtered.update(android)
    filtered.update(core)
    n_android = sum([1 for tags in new_tags.values() if "android" in tags])
    n_ios = sum([1 for tags in new_tags.values() if "ios" in tags])

    logging.info("Total strings grepped: {0}\tiOS: {1}\tandroid: {2}".format(
        len(filtered), n_android, n_ios))

    strings_txt = StringsTxt(
        "{0}/{1}".format(OMIM_ROOT, StringsTxt.STRINGS_TXT_PATH))
    logging.info("Total strings in strings.txt: {0}".format(
        len(strings_txt.translations)))

    strings_txt.translations = {
        key: dict(strings_txt.translations[key]) for key in filtered}

    strings_txt.comments_tags_refs = new_comments_and_tags(
        strings_txt, filtered, new_tags)

    path = args.output if isabs(
        args.output) else "{0}/{1}".format(OMIM_ROOT, args.output)
    strings_txt.write_formatted(target_file=path, langs=args.langs)

    if args.generate:
        exec_shell(
            "{}/unix/generate_localizations.sh".format(OMIM_ROOT),
            args.output, args.output
        )


def find_unused():
    core = grep_core()
    ios = grep_ios()
    android = grep_android()

    filtered = ios
    filtered.update(android)
    filtered.update(core)

    strings_txt = StringsTxt(
        "{0}/{1}".format(OMIM_ROOT, StringsTxt.STRINGS_TXT_PATH))
    unused = set(strings_txt.translations.keys()) - filtered
    if len(unused):
        print("Translation definitions/keys which are no longer used in the codebase:")
        print(*unused, sep="\n")
    else:
        print("There are no unused translation definitions/keys.")
    return len(unused)


def do_missing(args):
    ios = set(grep_ios())
    strings_txt_keys = set(StringsTxt().translations.keys())
    missing = ios - strings_txt_keys

    if missing:
        for m in missing:
            logging.info(m)
        exit(1)
    logging.info("Ok. No missing strings.")
    exit(0)


def do_candidates(args):
    all_candidates = defaultdict(list)
    for source, candidate in grep_ios_candidates():
        all_candidates[candidate].append(source)

    for candidate, sources in all_candidates.items():
        print(candidate, sources)


def do_ios_suspects(args):
    grep = "grep -re -I 'L(' {}/iphone/*".format(OMIM_ROOT)
    suspects = exec_shell(grep)
    SUSPECT_RE = re.compile(r"(.*?):.*?\WL\(([^@].*?)\)")
    strings = strings_from_grepped(suspects, SUSPECT_RE)
    for s in strings:
        print(s)


def find_omim():
    my_path = abspath(__file__)
    tools_index = my_path.rfind("/tools/python")
    omim_path = my_path[:tools_index]
    return omim_path


def read_hardcoded_categories():
    categoriestxt = OMIM_ROOT + "/data/categories.txt"
    logging.info(f"Retrieving search categories from: {categoriestxt}")
    with open(categoriestxt) as infile:
        return [s.strip().lstrip('@') for s in infile if s.startswith("@category_")]


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    prog_name, args = get_args()

    OMIM_ROOT = args.omim_root

    HARDCODED_CATEGORIES = read_hardcoded_categories()
    logging.info(f"Loaded categories: {HARDCODED_CATEGORIES}")

    args.langs = set(args.langs) if args.langs else None

    if args.validate:
        if find_unused():
            print("ERROR: there are unused strings (run \"{0} -s\" to delete them)".format(prog_name))
            print("If you're running generate_localizations.sh, add -u to generate anyway.\nExiting...")
            exit(1)
        exit(0)

    if args.missing:
        do_missing(args)
        exit(0)

    if args.candidates:
        do_candidates(args)
        exit(0)

    if args.single:
        do_single(args)
    else:
        do_multiple(args)

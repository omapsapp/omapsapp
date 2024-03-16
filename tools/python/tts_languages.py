#!/usr/bin/env python3
from __future__ import print_function
from optparse import OptionParser
import re

LANGUAGES_HPP_TEMPLATE = """\
#pragma once

#include <array>
#include <string>

// This file is autogenerated while exporting sounds.csv from the google table.
// It contains the list of languages which can be used by TTS.
// It shall be included to Android(jni) and iOS part to get the languages list.

namespace routing
{{
namespace turns
{{
namespace sound
{{
std::array<std::string, {lang_list_size}> const kLanguageList =
{{{{
{lang_list}
}}}};
}}  // namespace sound
}}  // namespace turns
}}  // namespace routing
"""


def parse_args():
    opt_parser = OptionParser(
        description="Creates a language.hpp out of the sound.txt file.",
        usage="python %prog <path_to_sound.txt> <path_to_languages.hpp>",
        version="%prog 1.0"
    )
    (options, args) = opt_parser.parse_args()
    if len(args) != 2:
        opt_parser.error("Wrong number of arguments.")
    return args


def read_languages(strings_name):
    langs = set()
    RE_LANG = re.compile(r'^ *([\w-]+) *=')
    with open(strings_name, "r") as langs_file:
        for line in langs_file:
            m = RE_LANG.search(line)
            if m:
                langs.add(m.group(1))
    return langs


def make_languages_hpp(langs, hpp_name):
    print ("Creating {}".format(hpp_name))
    lang_str = ",\n".join(["  \"{}\"".format(language) for language in sorted(langs)])
    with open(hpp_name, "w") as hpp_file:
        hpp_file.write(LANGUAGES_HPP_TEMPLATE.format(lang_list_size=len(langs), lang_list=lang_str))


def run():
    strings_name, langs_hpp_name = parse_args()
    langs = read_languages(strings_name)
    make_languages_hpp(langs, langs_hpp_name)


if __name__ == "__main__":
    run()

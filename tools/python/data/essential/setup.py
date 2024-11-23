#!/usr/bin/env python3
import os
import sys

module_dir = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(module_dir, "..", ".."))

from data.base import get_version
from data.base import setup

setup(
    __file__,
    "essential",
    [
        "borders_vs_osm.csv",
        "categories_brands.txt",
        "categories_cuisines.txt",
        "categories.txt",
        "classificator.txt",
        "colors.txt",
        "countries_meta.txt",
        "countries_synonyms.csv",
        "countries.txt",
        "external_resources.txt",
        "fonts/blacklist.txt",
        "fonts/unicode_blocks.txt",
        "fonts/whitelist.txt",
        "hierarchy.txt",
        "mapcss-dynamic.txt",
        "mapcss-mapping.csv",
        "mixed_nodes.txt",
        "mixed_tags.txt",
        "mwm_names_en.txt",
        "old_vs_new.csv",
        "patterns.txt",
        "replaced_tags.txt",
        "skipped_elements.json",
        "synonyms.txt",
        "transit_colors.txt",
        "types.txt",
        "ugc_types.csv",
        "visibility.txt",
    ],
    install_requires=["omim-data-files=={}".format(get_version())]
)

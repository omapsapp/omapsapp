import collections
import datetime
import logging
import logging.config
import os
import shutil
import sys
from functools import wraps
from typing import Any
from typing import AnyStr
from typing import Callable
from typing import Dict
from typing import List
from typing import Optional
from typing import Set
from typing import Type
from typing import Union

from maps_generator.generator import settings
from maps_generator.generator import status
from maps_generator.generator.osmtools import build_osmtools
from maps_generator.generator.stages import Stage
from maps_generator.utils.file import find_executable
from maps_generator.utils.file import is_executable
from maps_generator.utils.file import make_symlink

logger = logging.getLogger("maps_generator")

WORLD_NAME = "World"
WORLD_COASTS_NAME = "WorldCoasts"

WORLDS_NAMES = {WORLD_NAME, WORLD_COASTS_NAME}


def get_all_countries_list(borders_path: AnyStr) -> List[AnyStr]:
    """Returns all countries including World and WorldCoasts."""
    return [
        f.replace(".poly", "")
        for f in os.listdir(borders_path)
        if os.path.isfile(os.path.join(borders_path, f))
    ] + list(WORLDS_NAMES)


def create_if_not_exist_path(path: AnyStr) -> bool:
    """Creates directory if it doesn't exist."""
    try:
        os.makedirs(path)
        logger.info(f"Create {path} ...")
        return True
    except FileExistsError:
        return False


def create_if_not_exist(func: Callable[..., AnyStr]) -> Callable[..., AnyStr]:
    """
    It's a decorator, that wraps func in create_if_not_exist_path,
    that returns a path.
    """

    @wraps(func)
    def wrapper(*args, **kwargs):
        path = func(*args, **kwargs)
        create_if_not_exist_path(path)
        return path

    return wrapper


class Version:
    """It's used for writing and reading a generation version."""

    @staticmethod
    def write(out_path: AnyStr, version: AnyStr):
        with open(os.path.join(out_path, settings.VERSION_FILE_NAME), "w") as f:
            f.write(str(version))

    @staticmethod
    def read(version_path: AnyStr) -> int:
        with open(version_path) as f:
            line = f.readline().strip()
            try:
                return int(line)
            except ValueError:
                logger.exception(f"Cast '{line}' to int error.")
                return 0


def find_last_build_dir(hint: Optional[AnyStr] = None) -> Optional[AnyStr]:
    """
    It tries to find a last generation directory. If it's found function
    returns path of last generation directory. Otherwise returns None.
    """
    if hint is not None:
        p = os.path.join(settings.MAIN_OUT_PATH, hint)
        return hint if os.path.exists(p) else None
    try:
        paths = [
            os.path.join(settings.MAIN_OUT_PATH, f)
            for f in os.listdir(settings.MAIN_OUT_PATH)
        ]
    except FileNotFoundError:
        logger.exception(f"{settings.MAIN_OUT_PATH} not found.")
        return None
    versions = []
    for path in paths:
        version_path = os.path.join(path, settings.VERSION_FILE_NAME)
        if not os.path.isfile(version_path):
            versions.append(0)
        else:
            versions.append(Version.read(version_path))
    pairs = sorted(zip(paths, versions), key=lambda p: p[1], reverse=True)
    return None if not pairs or pairs[0][1] == 0 else pairs[0][0].split(os.sep)[-1]


class PathProvider:
    """
    PathProvider is used for building paths for a maps generation.
    """

    def __init__(self, build_path: AnyStr, build_name:AnyStr, mwm_version: AnyStr):
        self.build_path = build_path
        self.build_name = build_name
        self.mwm_version = mwm_version

        create_if_not_exist_path(self.build_path)

    @property
    @create_if_not_exist
    def intermediate_data_path(self) -> AnyStr:
        """
        intermediate_data_path contains intermediate files,
        for example downloaded external files, that are needed for generation,
        *.mwm.tmp files, etc.
        """
        return os.path.join(self.build_path, "intermediate_data")

    @property
    @create_if_not_exist
    def cache_path(self) -> AnyStr:
        """cache_path contains caches for nodes, ways, relations."""
        if not settings.CACHE_PATH:
            return self.intermediate_data_path

        return os.path.join(settings.CACHE_PATH, self.build_name)

    @property
    @create_if_not_exist
    def data_path(self) -> AnyStr:
        """It's a synonym for intermediate_data_path."""
        return self.intermediate_data_path

    @property
    @create_if_not_exist
    def intermediate_tmp_path(self) -> AnyStr:
        """intermediate_tmp_path contains *.mwm.tmp files."""
        return os.path.join(self.intermediate_data_path, "tmp")

    @property
    @create_if_not_exist
    def mwm_path(self) -> AnyStr:
        """mwm_path contains *.mwm files."""
        return os.path.join(self.build_path, self.mwm_version)

    @property
    @create_if_not_exist
    def log_path(self) -> AnyStr:
        """mwm_path log files."""
        return os.path.join(self.build_path, "logs")

    @property
    @create_if_not_exist
    def generation_borders_path(self) -> AnyStr:
        """
        generation_borders_path contains *.poly files, that define
        which .mwm files are generated.
        """
        return os.path.join(self.intermediate_data_path, "borders")

    @property
    @create_if_not_exist
    def draft_path(self) -> AnyStr:
        """draft_path is used for saving temporary intermediate files."""
        return os.path.join(self.build_path, "draft")

    @property
    @create_if_not_exist
    def osm2ft_path(self) -> AnyStr:
        """osm2ft_path contains osmId<->ftId mappings."""
        return os.path.join(self.build_path, "osm2ft")

    @property
    @create_if_not_exist
    def coastline_path(self) -> AnyStr:
        """coastline_path is used for a coastline generation."""
        return os.path.join(self.intermediate_data_path, "coasts")

    @property
    @create_if_not_exist
    def coastline_tmp_path(self) -> AnyStr:
        """coastline_tmp_path is used for a coastline generation."""
        return os.path.join(self.coastline_path, "tmp")

    @property
    @create_if_not_exist
    def status_path(self) -> AnyStr:
        """status_path contains status files."""
        return os.path.join(self.build_path, "status")

    @property
    @create_if_not_exist
    def descriptions_path(self) -> AnyStr:
        return os.path.join(self.intermediate_data_path, "descriptions")

    @property
    @create_if_not_exist
    def stats_path(self) -> AnyStr:
        return os.path.join(self.build_path, "stats")

    @property
    @create_if_not_exist
    def transit_path(self) -> AnyStr:
        return self.intermediate_data_path

    @property
    def transit_path_experimental(self) -> AnyStr:
        return (
            os.path.join(self.intermediate_data_path, "transit_from_gtfs")
            if settings.TRANSIT_URL
            else ""
        )

    @property
    def world_roads_path(self) -> AnyStr:
        return (
            os.path.join(self.intermediate_data_path, "world_roads.txt")
            if settings.NEED_BUILD_WORLD_ROADS
            else ""
        )

    @property
    def planet_osm_pbf(self) -> AnyStr:
        return os.path.join(self.build_path, f"{settings.PLANET}.osm.pbf")

    @property
    def planet_o5m(self) -> AnyStr:
        return os.path.join(self.build_path, f"{settings.PLANET}.o5m")

    @property
    def world_roads_o5m(self) -> AnyStr:
        return os.path.join(self.build_path, "world_roads.o5m")

    @property
    def main_status_path(self) -> AnyStr:
        return os.path.join(self.status_path, status.with_stat_ext("stages"))

    @property
    def packed_polygons_path(self) -> AnyStr:
        return os.path.join(self.mwm_path, "packed_polygons.bin")

    @property
    def localads_path(self) -> AnyStr:
        return os.path.join(self.build_path, f"localads_{self.mwm_version}")

    @property
    def types_path(self) -> AnyStr:
        return os.path.join(self.user_resource_path, "types.txt")

    @property
    def external_resources_path(self) -> AnyStr:
        return os.path.join(self.mwm_path, "external_resources.txt")

    @property
    def id_to_wikidata_path(self) -> AnyStr:
        return os.path.join(self.intermediate_data_path, "id_to_wikidata.csv")

    @property
    def wiki_url_path(self) -> AnyStr:
        return os.path.join(self.intermediate_data_path, "wiki_urls.txt")

    @property
    def ugc_path(self) -> AnyStr:
        return os.path.join(self.intermediate_data_path, "ugc_db.sqlite3")

    @property
    def hotels_path(self) -> AnyStr:
        return os.path.join(self.intermediate_data_path, "hotels.csv")

    @property
    def promo_catalog_cities_path(self) -> AnyStr:
        return os.path.join(self.intermediate_data_path, "promo_catalog_cities.json")

    @property
    def promo_catalog_countries_path(self) -> AnyStr:
        return os.path.join(self.intermediate_data_path, "promo_catalog_countries.json")

    @property
    def popularity_path(self) -> AnyStr:
        return os.path.join(self.intermediate_data_path, "popular_places.csv")

    @property
    def subway_path(self) -> AnyStr:
        return os.path.join(
            self.intermediate_data_path, "mapsme_osm_subways.transit.json"
        )

    @property
    def food_paths(self) -> AnyStr:
        return os.path.join(self.intermediate_data_path, "ids_food.json")

    @property
    def food_translations_path(self) -> AnyStr:
        return os.path.join(self.intermediate_data_path, "translations_food.json")

    @property
    def cities_boundaries_path(self) -> AnyStr:
        return os.path.join(self.intermediate_data_path, "cities_boundaries.bin")

    @property
    def hierarchy_path(self) -> AnyStr:
        return os.path.join(self.user_resource_path, "hierarchy.txt")

    @property
    def old_to_new_path(self) -> AnyStr:
        return os.path.join(self.user_resource_path, "old_vs_new.csv")

    @property
    def borders_to_osm_path(self) -> AnyStr:
        return os.path.join(self.user_resource_path, "borders_vs_osm.csv")

    @property
    def countries_synonyms_path(self) -> AnyStr:
        return os.path.join(self.user_resource_path, "countries_synonyms.csv")

    @property
    def counties_txt_path(self) -> AnyStr:
        return os.path.join(self.mwm_path, "countries.txt")

    @property
    def user_resource_path(self) -> AnyStr:
        return settings.USER_RESOURCE_PATH

    @staticmethod
    def srtm_path() -> AnyStr:
        return settings.SRTM_PATH

    @staticmethod
    def isolines_path() -> AnyStr:
        return settings.ISOLINES_PATH

    @staticmethod
    def addresses_path() -> AnyStr:
        return settings.ADDRESSES_PATH

    @staticmethod
    def borders_path() -> AnyStr:
        return os.path.join(settings.USER_RESOURCE_PATH, "borders")

    @staticmethod
    @create_if_not_exist
    def tmp_dir():
        return settings.TMPDIR


COUNTRIES_NAMES = set(get_all_countries_list(PathProvider.borders_path()))


class Env:
    """
    Env provides a generation environment. It sets up instruments and paths,
    that are used for a maps generation. It stores state of the maps generation.
    """

    def __init__(
        self,
        countries: Optional[List[AnyStr]] = None,
        production: bool = False,
        build_name: Optional[AnyStr] = None,
        build_suffix: AnyStr = "",
        skipped_stages: Optional[Set[Type[Stage]]] = None,
        force_download_files: bool = False,
    ):
        self.setup_logging()

        logger.info("Start setup ...")
        os.environ["TMPDIR"] = PathProvider.tmp_dir()
        for k, v in self.setup_osm_tools().items():
            setattr(self, k, v)

        self.production = production
        self.force_download_files = force_download_files
        self.countries = countries
        self.skipped_stages = set() if skipped_stages is None else skipped_stages
        if self.countries is None:
            self.countries = get_all_countries_list(PathProvider.borders_path())

        self.node_storage = settings.NODE_STORAGE

        version_format = "%Y_%m_%d__%H_%M_%S"
        suffix_div = "-"
        self.dt = None
        if build_name is None:
            self.dt = datetime.datetime.now()
            build_name = self.dt.strftime(version_format)
            if build_suffix:
                build_name = f"{build_name}{suffix_div}{build_suffix}"
        else:
            s = build_name.split(suffix_div, maxsplit=1)
            if len(s) == 1:
                s.append("")

            date_str, build_suffix = s
            self.dt = datetime.datetime.strptime(date_str, version_format)

        self.build_suffix = build_suffix
        self.mwm_version = self.dt.strftime("%y%m%d")
        self.planet_version = self.dt.strftime("%s")
        self.build_path = os.path.join(settings.MAIN_OUT_PATH, build_name)
        self.build_name = build_name

        self.gen_tool = self.setup_generator_tool()
        if WORLD_NAME in self.countries:
            self.world_roads_builder_tool = self.setup_world_roads_builder_tool()
        self.diff_tool = self.setup_mwm_diff_tool()

        logger.info(f"Build name is {self.build_name}.")
        logger.info(f"Build path is {self.build_path}.")

        self.paths = PathProvider(self.build_path, self.build_name, self.mwm_version)

        Version.write(self.build_path, self.planet_version)
        self.setup_borders()
        self.setup_osm2ft()

        if self.force_download_files:
            for item in os.listdir(self.paths.status_path):
                if item.endswith(".download"):
                    os.remove(os.path.join(self.paths.status_path, item))

        self.main_status = status.Status()
        # self.countries_meta stores log files and statuses for each country.
        self.countries_meta = collections.defaultdict(dict)
        self.subprocess_out = None
        self.subprocess_countries_out = {}

        printed_countries = ", ".join(self.countries)
        if len(self.countries) > 50:
            printed_countries = (
                f"{', '.join(self.countries[:25])}, ..., "
                f"{', '.join(self.countries[-25:])}"
            )
        logger.info(
            f"The following {len(self.countries)} maps will build: "
            f"{printed_countries}."
        )
        logger.info("Finish setup")

    def __getitem__(self, item):
        return self.__dict__[item]

    def get_tmp_mwm_names(self) -> List[AnyStr]:
        tmp_ext = ".mwm.tmp"
        existing_names = set()
        for f in os.listdir(self.paths.intermediate_tmp_path):
            path = os.path.join(self.paths.intermediate_tmp_path, f)
            if f.endswith(tmp_ext) and os.path.isfile(path):
                name = f.replace(tmp_ext, "")
                if name in self.countries:
                    existing_names.add(name)
        return [c for c in self.countries if c in existing_names]

    def add_skipped_stage(self, stage: Union[Type[Stage], Stage]):
        if isinstance(stage, Stage):
            stage = stage.__class__
        self.skipped_stages.add(stage)

    def is_accepted_stage(self, stage: Union[Type[Stage], Stage]) -> bool:
        if isinstance(stage, Stage):
            stage = stage.__class__
        return stage not in self.skipped_stages

    def finish(self):
        self.main_status.finish()

    def finish_mwm(self, mwm_name: AnyStr):
        self.countries_meta[mwm_name]["status"].finish()

    def set_subprocess_out(self, subprocess_out: Any, country: Optional[AnyStr] = None):
        if country is None:
            self.subprocess_out = subprocess_out
        else:
            self.subprocess_countries_out[country] = subprocess_out

    def get_subprocess_out(self, country: Optional[AnyStr] = None):
        if country is None:
            return self.subprocess_out
        else:
            return self.subprocess_countries_out[country]

    @staticmethod
    def setup_logging():
        def exception_handler(type, value, tb):
            logger.exception(
                f"Uncaught exception: {str(value)}", exc_info=(type, value, tb)
            )

        logging.config.dictConfig(settings.LOGGING)
        sys.excepthook = exception_handler

    @staticmethod
    def setup_generator_tool() -> AnyStr:
        logger.info("Check generator tool ...")
        exceptions = []
        for gen_tool in settings.POSSIBLE_GEN_TOOL_NAMES:
            gen_tool_path = shutil.which(gen_tool)
            if gen_tool_path is None:
                logger.info(f"Looking for generator tool in {settings.BUILD_PATH} ...")
                try:
                    gen_tool_path = find_executable(settings.BUILD_PATH, gen_tool)
                except FileNotFoundError as e:
                    exceptions.append(e)
                    continue

            logger.info(f"Generator tool found - {gen_tool_path}")
            return gen_tool_path

        raise Exception(exceptions)

    @staticmethod
    def setup_world_roads_builder_tool() -> AnyStr:
        logger.info(f"Check world_roads_builder_tool. Looking for it in {settings.BUILD_PATH} ...")
        world_roads_builder_tool_path = find_executable(settings.BUILD_PATH, "world_roads_builder_tool")
        logger.info(f"world_roads_builder_tool found - {world_roads_builder_tool_path}")
        return world_roads_builder_tool_path

    @staticmethod
    def setup_mwm_diff_tool() -> AnyStr:
        logger.info(f"Check mwm_diff_tool. Looking for it in {settings.BUILD_PATH} ...")
        mwm_diff_tool_path = find_executable(settings.BUILD_PATH, "mwm_diff_tool")
        logger.info(f"mwm_diff_tool found - {mwm_diff_tool_path}")
        return mwm_diff_tool_path

    @staticmethod
    def setup_osm_tools() -> Dict[AnyStr, AnyStr]:
        path = settings.OSM_TOOLS_PATH
        osm_tool_names = [
            settings.OSM_TOOL_CONVERT,
            settings.OSM_TOOL_UPDATE,
            settings.OSM_TOOL_FILTER,
        ]

        logger.info("Check for the osmctools binaries...")

        # Check in the configured path first.
        tmp_paths = [os.path.join(path, t) for t in osm_tool_names]
        if not all([is_executable(t) for t in tmp_paths]):
            # Or use a system-wide installation.
            tmp_paths = [shutil.which(t) for t in osm_tool_names]
        if all([is_executable(t) for t in tmp_paths]):
            osm_tool_paths = dict(zip(osm_tool_names, tmp_paths))
            logger.info(f"Found osmctools at {', '.join(osm_tool_paths.values())}")
            return osm_tool_paths

        logger.info(f"osmctools are not found, building from the sources into {path}...")
        os.makedirs(path, exist_ok=True)
        return build_osmtools(settings.OSM_TOOLS_SRC_PATH)

    def setup_borders(self):
        temp_borders = self.paths.generation_borders_path
        borders = PathProvider.borders_path()
        for x in self.countries:
            if x in WORLDS_NAMES:
                continue

            poly = f"{x}.poly"
            make_symlink(os.path.join(borders, poly), os.path.join(temp_borders, poly))
        make_symlink(temp_borders, os.path.join(self.paths.draft_path, "borders"))

    def setup_osm2ft(self):
        for x in os.listdir(self.paths.osm2ft_path):
            p = os.path.join(self.paths.osm2ft_path, x)
            if os.path.isfile(p) and x.endswith(".mwm.osm2ft"):
                shutil.move(p, os.path.join(self.paths.mwm_path, x))

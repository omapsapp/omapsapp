import re
from typing import Optional, Tuple, Dict, List

LANGUAGES = [
    "af", "ar", "be", "bg", "ca", "cs", "da", "de", "el", "en", "en-GB", "es", "es-MX", "et",
    "eu", "fa", "fi", "fr", "fr-CA", "he", "hi", "hu", "id", "it", "ja", "ko", "lt", "mr", "nb",
    "nl", "pl", "pt", "pt-BR", "ro", "ru", "sk", "sv", "sw", "th", "tr", "uk", "vi", "zh-Hans", "zh-Hant"
]


class AbstractParser:
    def __init__(self, keys: List[str]):
        self.keys = keys

    def parse_line(self, line: str) -> Optional[Tuple[str, str]]:
        raise NotImplementedError("You must implement parse_line.")

    def match_category(self, line: str, result: Dict[str, Dict]):
        category_match = self.category().search(line)
        if category_match:
            category = category_match.group(1)
            if category in self.keys:
                if category not in result:
                    result[category] = {}

    def parse_file(self, filename: str) -> Dict[str, Dict]:
        result = {}
        current_category = None

        with open(filename, "r", encoding="utf-8") as file:
            for line in file:
                line = line.strip()

                # Skip comments and empty lines
                if self.should_exclude_line(line):
                    continue

                # Match a new category
                category_match = self.category().match(line)
                if category_match:
                    current_category = category_match.group(1)
                    if current_category not in result:
                        result[current_category] = {}
                    continue

                # Parse translations for the current category
                if current_category:
                    parsed = self.parse_line(line)
                    if parsed:
                        lang, translation = parsed
                        result[current_category].setdefault(lang, []).append(translation)

        return result

    def category(self) -> re.Pattern:
        raise NotImplementedError("You must implement category.")

    def is_new_category(self, line: str) -> bool:
        return bool(self.category().match(line))

    def extract_category(self, line: str) -> Optional[str]:
        match = self.category().match(line)
        return match.group(1) if match else None

    def should_exclude_line(self, line: str) -> bool:
        return False


class CategoriesParser(AbstractParser):
    def parse_line(self, line: str) -> Optional[Tuple[str, str]]:
        line_match = re.match(r"^([^:]+):(.+)$", line)
        if line_match:
            lang = line_match.group(1).strip()
            translation = line_match.group(2).strip()
            return lang, translation
        return None

    def category(self) -> re.Pattern:
        return re.compile(r"^([a-zA-Z0-9_-]+)\|@(.+)$")

    def should_exclude_line(self, line: str) -> bool:
        return line.startswith("#") or not line


class StringsParser(AbstractParser):
    def parse_line(self, line: str) -> Optional[Tuple[str, str]]:
        line_match = re.match(r"^([^=]+)=(.*)$", line)
        if line_match:
            lang = line_match.group(1).strip()
            translation = line_match.group(2).strip()
            return lang, translation
        return None

    def category(self) -> re.Pattern:
        return re.compile(r"^\[([a-zA-Z0-9_]+)]$")

    def should_exclude_line(self, line: str) -> bool:
        return line.startswith("tags") or not line

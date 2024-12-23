#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
from typing import List, Dict, Optional


def get_string_from_file(filename: str) -> List[str]:
    strings = []
    with open(filename, 'r') as f:
        for string in f:
            strings.append(string.strip())
    return strings


def get_data_inside_brackets(line: str) -> Optional[str]:
    start = line.find('[')
    end = line.find(']')

    if start != -1 and end != -1 and start < end:
        return line[start + 1:end]
    return None


def is_all_translations_found(localized_strings: Dict[str, str], strings: List[str]):
    missing_keys = [key for key in strings if key not in localized_strings]

    if missing_keys:
        raise Exception(f'Could not find translations for strings: {missing_keys}')


def get_localized_strings(strings: List[str], strings_dir: Path, locale: str = "en") -> Dict[str, str]:
    localized_string_prefix = f'{locale} = '
    localized_strings = {}
    for file in strings_dir.iterdir():
        if not file.suffix == '.txt':
            continue
        with file.open('r') as f:
            for line in f:
                str_name = get_data_inside_brackets(line)
                if str_name and str_name in strings:
                    for translation in f:
                        if translation.strip() == '':
                            break
                        prefix_pos = translation.find(localized_string_prefix)
                        if prefix_pos != -1:
                            localized_string = translation[prefix_pos + len(localized_string_prefix):].strip()
                            localized_strings[str_name] = localized_string

    is_all_translations_found(localized_strings, strings)

    return localized_strings


def generate_output_file_from_template(localized_strings: Dict[str, str], template: Path) -> str:
    template_data = template.read_text()

    generated_data = ""
    for key in localized_strings:
        key_name = f'k{"".join([part.capitalize() for part in key.split("_")])}'
        generated_data += f'static const std::string {key_name} = "{key}";\n'
    template_data = template_data.replace('@GENERATE_CONSTANTS@\n', generated_data)

    generated_data = ""
    for key, value in localized_strings.items():
        generated_data += f'  def_values["{key}"] = "{value}";\n'

    return template_data.replace('@GENERATE_INIT@\n', generated_data)


def save_file(data: str, output: Path) -> None:
    if output.exists():
        os.remove(output)
    output.write_text(data)


def main():
    # Create argument parser
    parser = argparse.ArgumentParser(description="Generate core localizations header.")

    # Define required arguments
    parser.add_argument('--template', required=True, help='Output file template')
    parser.add_argument('--strings-dir', required=True, help='Directory containing the strings data')
    parser.add_argument('--generate', required=True, help='File path with strings to generate')
    parser.add_argument('--output', required=True, help='Output file path for the generated localizations header')

    # Parse arguments
    args = parser.parse_args()

    template_file = Path(args.template)
    strings_dir = Path(args.strings_dir)
    strings_to_generate = Path(args.generate)
    output_file = Path(args.output)

    if not os.path.isfile(template_file):
        raise Exception(f"The output template file {template_file} does not exist.")

    if not os.path.isdir(strings_dir):
        raise Exception(f"The strings directory '{strings_dir}' does not exist.\n")

    if not os.path.isfile(strings_to_generate):
        raise Exception(f"The strings to generate file {strings_to_generate} does not exist.")

    strings: List[str] = get_string_from_file(args.generate)
    print(f"Generating localizations for: {', '.join(strings)}")
    localized_strings: Dict[str, str] = get_localized_strings(strings, strings_dir)
    output_file_data: str = generate_output_file_from_template(localized_strings, template_file)
    save_file(output_file_data, output_file)


if __name__ == "__main__":
    main()

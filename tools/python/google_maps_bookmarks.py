#!/usr/bin/env python3

import csv
import json
import argparse
import mimetypes
import traceback
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from os import path, access, R_OK, linesep
from io import StringIO
from datetime import datetime

class GoogleMapsConverter:
    def __init__(self, input_file=None, output_format=None, bookmark_list_name=None, api_key=None):
        print("Follow these steps to export your saved places from Google Maps and convert them to a GPX or KML File")
        print()
        print("1. Create an API key for Google Places API following this guide")
        print("   https://developers.google.com/maps/documentation/places/web-service/get-api-key")
        print("2. Go to https://takeout.google.com/ and sign in with your Google account")
        print("3. Select 'Saved' and 'Maps (My Places)' and create an export")
        print("4. Download and unzip the export")
        print ("5a. Look for CSV files (e.g. for lists) in the folder Takeout/Saved")
        print ("5b. Look for GeoJSON files (e.g. for Saved Places) in the folder Takeout/Maps")
        print()
        
        if input_file is None:
            self.get_input_file()
        else:
            self.input_file = input_file
            if not path.isfile(self.input_file):
                raise FileNotFoundError(f"Couldn't find {self.input_file}")
            if not access(self.input_file, R_OK):
                raise PermissionError(f"Couldn't read {self.input_file}")

        if output_format is None:
            self.get_output_format()
        else:
            self.output_format = output_format

        if bookmark_list_name is None:
            self.get_bookmark_list_name()
        else:
            self.bookmark_list_name = bookmark_list_name
            self.output_file = self.bookmark_list_name + "." + self.output_format

        if api_key is None:
            self.get_api_key()
        else:
            self.api_key = api_key
        
        self.places = []

    def get_input_file(self):
        while True:
            self.input_file = input("Path to the file: ")
            if not path.isfile(self.input_file):
                print(f"Couldn't find {self.input_file}")
                continue
            if not access(self.input_file, R_OK):
                print(f"Couldn't read {self.input_file}")
                continue
            break
    
    def get_output_format(self):
        while True:
            self.output_format = input("Output format (kml or gpx): ").lower()
            if self.output_format not in ['kml', 'gpx']:
                print("Please provide a valid output format" + linesep)
                continue
            else:
                break
    
    def get_bookmark_list_name(self):
        while True:
            self.bookmark_list_name = input("Bookmark list name: ")
            if not self.bookmark_list_name:
                print("Please provide a name" + linesep)
                continue
            else:
                self.output_file = self.bookmark_list_name + "." + self.output_format
                break
            
    def get_api_key(self):
        while True:
            if self.api_key:
                break
            self.api_key = input("API key: ")
            if not self.api_key:
                print("Please provide an API key" + linesep)
                continue
            else:
                break
            
    def convert_timestamp(self, timestamp):
        if timestamp.endswith('Z'):
            timestamp = timestamp[:-1]
        date = datetime.fromisoformat(timestamp)
        return date.strftime('%Y-%m-%d %H:%M:%S')
        
    def get_json(self, url):
        max_attempts = 3
        for retry in range(max_attempts):
            try:
                response = urllib.request.urlopen(url)
                return json.load(response)
            except urllib.error.URLError:
                print(f"Couldn't connect to Google Maps. Retrying... ({retry + 1}/{max_attempts})")
                if retry < max_attempts - 1:
                    continue
                else:
                    raise

    def get_name_and_coordinates_from_google_api(self, api_key, q=None, cid=None):
        url = None
        if q:
            params = {'query': q, 'key': api_key}
            url = f"https://maps.googleapis.com/maps/api/place/textsearch/json?{urllib.parse.urlencode(params)}"   
        elif cid:
            params = {'cid': cid, 'fields': 'geometry,name', 'key': api_key}
            url= f"https://maps.googleapis.com/maps/api/place/details/json?{urllib.parse.urlencode(params)}"
        else:
            return None

        result = self.get_json(url)
        if result['status'] == 'OK':
            place = result.get('results', [result.get('result')])[0]
            location = place['geometry']['location']
            name = place['name']
            return {'name': name, 'coordinates': [str(location['lat']), str(location['lng'])]}
        else:
            print(f'{result.get("status", "")}: {result.get("error_message", "")}')
            return None     
                
    def process_geojson_features(self, content):
        try:
            geojson = json.loads(content)
        except json.JSONDecodeError:
            raise ValueError(f"The file {self.input_file} is not a valid JSON file.")
        for feature in geojson['features']:
            geometry = feature['geometry']
            coordinates = geometry['coordinates']
            properties = feature['properties']
            google_maps_url = properties.get('google_maps_url', '')
            location = properties.get('location', {})
            name = None
            
            # Check for "null island" coordinates [0, 0]
            # These are a common artifact of Google Maps exports
            # See https://github.com/organicmaps/organicmaps/pull/8721
            if coordinates == [0, 0]:
                parsed_url = urllib.parse.urlparse(google_maps_url)
                query_params = urllib.parse.parse_qs(parsed_url.query)
                # Google Maps URLs can contain either a query string parameter 'q', 'cid'
                q = query_params.get('q', [None])[0]
                cid = query_params.get('cid', [None])[0]
                # Sometimes the 'q' parameter is a comma-separated lat long pair
                if q and ',' in q and all(part.replace('.', '', 1).replace('-', '', 1).isdigit() for part in q.split(',')):
                    coordinates = q.split(',')
                else:
                    result = self.get_name_and_coordinates_from_google_api(self.api_key, q=q, cid=cid)
                    if result:
                        coordinates = result['coordinates']
                        if 'name' in result:
                            name = result['name']
                    else:
                        print(f"Couldn't extract coordinates from Google Maps. Skipping {q or cid}")
                    
            coord_string = ', '.join(map(str, coordinates)) if coordinates else None
            # If name was not retrieved from the Google Maps API, then use the name from the location object,
            # with a fallback to the address, and finally to the coordinates
            if not name:
                name = location.get('name') or location.get('address') or coord_string
                
            description = ""
            if 'address' in properties:
                description += f"<b>Address:</b> {location['address']}<br>"
            if 'date' in properties:
                description += f"<b>Date bookmarked:</b> {self.convert_timestamp(properties['date'])}<br>"
            if 'Comment' in properties:
                description += f"<b>Comment:</b> {properties['Comment']}<br>"
            if google_maps_url:
                description += f"<b>Google Maps URL:</b> <a href=\"{google_maps_url}\">{google_maps_url}</a><br>"

            place = {
                'name': name,
                'description': description
            }
            if coordinates:
                place['coordinates'] = ','.join(map(str, coordinates))
            else:
                place['coordinates'] = '0,0'
            self.places.append(place)

    def process_csv_features(self, content):
        csvreader = csv.reader(StringIO(content), delimiter=',')
        next(csvreader)  # skip header
        for idx, row in enumerate(csvreader):
            name = row[0]
            description = row[1]
            url = row[2]
            print(f"\rProgress: {idx + 1} Parsing {name}...", end='')
            try:
                if url.startswith("https://www.google.com/maps/search/"):
                    coordinates = url.split('/')[-1].split(',')
                    coordinates.reverse()
                    coordinates = ','.join(coordinates)
                elif url.startswith('https://www.google.com/maps/place/'):
                    ftid = url.split('!1s')[-1]
                    params = {'key': self.api_key, 'fields': 'geometry', 'ftid': ftid}
                    places_url = "https://maps.googleapis.com/maps/api/place/details/json?" \
                                 + urllib.parse.urlencode(params)
                    try:
                        data = self.get_json(places_url)
                        location = data['result']['geometry']['location']
                        coordinates = ','.join([str(location['lng']), str(location['lat'])])
                    except (urllib.error.URLError, KeyError):
                        print(f"Couldn't extract coordinates from Google Maps. Skipping {name}")
                        continue
                else:
                    print(f"Couldn't parse url. Skipping {name}")
                    continue

                self.places.append({'name': name, 'description': description, 'coordinates': coordinates})
            except Exception:
                print(f"Couldn't parse {name}: {traceback.format_exc()}")   

    def write_kml(self):
        root = ET.Element("kml")
        doc = ET.SubElement(root, "Document")
        for place in self.places:
            placemark = ET.SubElement(doc, "Placemark")
            ET.SubElement(placemark, "name").text = place['name']
            ET.SubElement(placemark, "description").text = place['description']
            point = ET.SubElement(placemark, "Point")
            ET.SubElement(point, "coordinates").text = place['coordinates']
        tree = ET.ElementTree(root)
        tree.write(self.output_file)

    def write_gpx(self):
        gpx = ET.Element("gpx", version="1.1", creator="GoogleMapsConverter")
        for place in self.places:
            wpt = ET.SubElement(gpx, "wpt", lat=place['coordinates'].split(',')[1], lon=place['coordinates'].split(',')[0])
            ET.SubElement(wpt, "name").text = place['name']
            ET.SubElement(wpt, "desc").text = place['description']
        tree = ET.ElementTree(gpx)
        tree.write(self.output_file)

    def convert(self):
        with open(self.input_file, 'r') as file:
            content = file.read().strip()
            if not content:
                raise ValueError(f"The file {self.input_file} is empty or not a valid JSON file.")
            
            mime_type, _ = mimetypes.guess_type(self.input_file)
            if mime_type == 'application/geo+json' or mime_type == 'application/json':
                self.process_geojson_features(content)
            elif mime_type == 'text/csv':
                self.process_csv_features(content)
            else:
                raise ValueError(f"Unsupported file format: {self.input_file}")
        
        # Write to output file in the desired format, KML or GPX
        if self.output_format == 'kml':
            self.write_kml()
        elif self.output_format == 'gpx':
            self.write_gpx()
        print("Exported Google Saved Places to " + path.abspath(self.output_file))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert Google Maps saved places to KML or GPX.")
    parser.add_argument('--input', help="Path to the file")
    parser.add_argument('--format', choices=['kml', 'gpx'], default='gpx', help="Output format: 'kml' or 'gpx'")
    parser.add_argument('--bookmark_list_name', help="Name of the bookmark list")
    parser.add_argument('--api_key', help="API key for Google Places API")
    args = parser.parse_args()

    converter = GoogleMapsConverter(
        input_file=args.input, 
        output_format=args.format, 
        bookmark_list_name=args.bookmark_list_name, 
        api_key=args.api_key
    )
    converter.convert()
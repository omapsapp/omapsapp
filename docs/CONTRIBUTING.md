# Contributing

## Issues

The simplest way to contribute is to [submit an issue](https://github.com/omapsapp/omapsapp/issues).
Please give developers as much information as possible: OS and application versions,
list of actions leading to a bug, a log file produced by the app.

When using the OMaps app on a device, use the built-in "Report a bug" option:
it creates a new e-mail with a log file attached. Your issue will be processed much
faster if you send it to bugs@omaps.app.

## Initializing the Repository

To initialize the repository, run `configure.sh`:

    ./configure.sh

## Setting up IDE

See [INSTALL.md](INSTALL.md) for command-line compilation instructions.

* Install XCode and Command Line Tools, then run XCode and click "I Agree".

### Qt Creator

* Download [Qt Creator](http://www.qt.io/download-open-source/) with Qt 5.
* In `Qt/5.5/clang_64/mkspecs/qdevice.pri` replace `10.10` with `10.11`, and
    add a line `QMAKE_MAC_SDK = macosx10.11` (see installed versions in
    `/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/`)
* Start Qt Creator, open `omim.pro`, choose "qt" target and run the project.
* To build the project faster, open "Project Settings", find "Build Steps", then
    "Make Arguments" and put "-j8" there (without quotes).

Debugging may not work in Qt Creator. To enable it, try creating a Build & Run kit in
preferences with XCode's lldb as a debugger and a clang compiler.

At the moment configuration assumes you've cloned repository into omim (also a default name) directory.
If you are using shadow-dir for building artifacts (default behavior) and custom directory for repo -
you'll need to create a "data" symlink in the shadow-dir path to the /repo/data directory.

### XCode

* Install [Homebrew](http://brew.sh/) and run `brew install qt5`.
* Run XCode, open `xcode/omim.xcworkspace`.
* Select "xcOMaps" scheme and run the product.

## Guidelines

- [C++ Style Guide](CPP_STYLE.md) - use `clang-format` when in doubt.
- [Objective-C Style Guide](OBJC_STYLE.md).
- [How to write a commit message](COMMIT_MESSAGES.md).

## Pull Requests

All contributions to OMaps source code should be submitted via github pull requests.
Each pull request is reviewed by OMaps maintainers, to ensure consistent code style
and quality. Sometimes the review process even for smallest commits can be
very thorough. Please follow [the developer guidelines](#guidelines).

To contribute you must assure that you have read and are following the rules
stated in the [Developers Certificate of Origin](DCO.md) (DCO). We have
borrowed this procedure from the Linux kernel project to improve tracking of
who did what, and for legal reasons.

To sign-off a patch, just add a line in the commit message saying:

    Signed-off-by: Some Developer somedev@example.com

Git has a flag that can sign a commit for you. An example using it is:

    git commit -s -m 'An example commit message'

Use your real name or on some rare cases a company email address, but we
disallow pseudonyms or anonymous contributions.

## Directories

### Core

* `api` - external API of the application.
* `base` - some base things, like macros, logging, caches etc.
* `coding` - I/O classes and data processing.
* `drape` - the new graphics library core.
* `drape_frontend` - scene and resource manager for the Drape library.
* `generator` - map building tool.
* `geocoder` -
* `geometry` - geometry primitives we use.
* `indexer` - processor for map files, classificator, styles.
* `map` - app business logic, including a scene manager.
* `platform` - platform abstraction classes: file paths, http requests, location services.
* `routing` - in-app routing engine.
* `routing_common` -
* `search` - ranking and searching classes.
* `std` - standard headers wrappers, for Boost, STL, C-rt.
* `storage` - map reading function.
* `tracking` -
* `traffic` - real-time traffic information.
* `transit` -
* `ugc` - user generated content, such as reviews.

### Other

Some of these contain their own README files.

* `3party` - external libraries, sometimes modified.
* `android` - Android UI.
* `cmake` - CMake helper files.
* `data` - data files for the application: maps, styles, country borders.
* `debian` - package sources for Debian.
* `descriptions` -
* `editor` -
* `feature_list` -
* `installer` - long-abandoned installer for Windows.
* `iphone` - iOS UI.
* `kml` - manipulation of KML files.
* `mapshot` - generate screenshots of maps, specified by coordinates and zoom level.
* `metrics` -
* `openlr` -
* `partners_api` - API for partners of the OMaps project.
* `pyhelpers` -
* `qt` - desktop application.
* `qt_tstfrm` - widgets for visual testing.
* `shaders` - shaders for rendering.
* `skin_generator` - a console app for building skin files with icons and symbols.
* `software_renderer` -
* `stats` - Alohalytics statistics.
* `testing` - common interfaces for tests.
* `tools` - tools for building packages and maps, for testing etc.
* `track_analyzing` -
* `track_generator` - Generate smooth tracks based on waypoints from KML.
* `xcode` - XCode workspace.

## Questions?

For any questions about developing OMaps and relevant services - virtually about anything related,
please write to us at bugs@omaps.app, we'll be happy to help.

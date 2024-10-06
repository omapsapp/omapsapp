# Building

- [System requirements](#system-requirements)
- [Getting sources](#getting-sources)
- [Desktop](#desktop-app)
- [Android](#android-app)
- [iOS](#ios-app)
- [Map data and styles](#map-data-and-styles)

## System requirements

To build and run Organic Maps you'll need a machine with at least 4Gb of RAM and 20-30Gb of disk space depending on your target platform. Expect to download 5-10Gb of files.

For _Windows_ you need to have [Git for Windows](https://git-scm.com/download/win) installed and Git bash available in the PATH.

## Getting sources

First of all get the source code. The full Organic Maps sources repository is ~8.5Gb in size, there are various [clone options](#special-cases-options) to reduce the download size to suit your needs.

For _Windows_, it's necessary to enable symlink support:
1. Activate _Windows Development Mode_ to enable symlinks globally:
  - Windows 10: _Settings_ -> _Update and Security_ -> _For Developers_ -> _Activate Developer Mode_
  - Windows 11: _Settings_ -> _Privacy and Security_ -> _For Developers_ -> _Activate Developer Mode_
2. Enable [symlinks](https://git-scm.com/docs/git-config#Documentation/git-config.txt-coresymlinks) support in git. The easiest way is to reinstall the latest [Git for Windows](https://git-scm.com/download/win) with the "Enable Symlinks" checkbox checked. If you don't want to reinstall Git, then you can add `-c core.symlinks=true` parameter to the clone command below to enable symlinks for the repository.

```bash
git config --global core.symlinks true
```

Clone the repository including all submodules (see [Special cases options](#special-cases-options) below):

(if you plan to contribute and propose pull requests then use a web interface at https://github.com/organicmaps/organicmaps to fork the repository first and use your fork's URL in the command below)

```bash
git clone --recurse-submodules https://github.com/organicmaps/organicmaps.git
```

Go into the cloned repository:
```bash
cd organicmaps
```

Configure the repository for an open source build:

(if you plan to publish the app privately in stores check [special options](#special-cases-options))

```bash
bash ./configure.sh
```

For _Windows 10_:  You should be able to build the project by following either of these setup methods:

**Setup 1: Using WSL**
1. Install [WSL](https://learn.microsoft.com/en-us/windows/wsl/install) on your machine.
2. Install g++ by running the following command in WSL: `sudo apt install g++`
3. Run `./configure.sh` in WSL.

**Setup 2: Using Visual Studio Developer Command Prompt**
1. Install the [Visual Studio Developer Command Prompt](https://docs.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell?view=vs-2022) (make sure to choose the latest MSVC x64/x86 build tool and Windows 10/11 SDK as individual components while installing Visual Studio).
2. Run the following command and follow instructions:

```bash
"C:\Program Files\Git\bin\bash.exe" configure.sh # execute the script by using Developer Command Prompt
```

### Special cases options

If you're only doing a one-off build or your internet bandwidth or disk space is limited, add following options to the `git clone` command:

- a `--filter=blob:limit=128k` option to make a _partial clone_ (saves ~4Gb), i.e. blob files over 128k in size will be excluded from the history and downloaded on-demand - is suitable for generic development.

- a `--depth=1` option to make a _shallow copy_ (and possibly a `--no-single-branch` to have all branches not just `master`), i.e. omit history while retaining current commits only (saves ~4.5Gb) - suitable for one-off builds.

- a `--shallow-submodules` option to _shallow clone_ the submodules (save ~1.3Gb) - this is suitable for a generic development if no work on submodules is planned.

If you mistakenly did a `git clone` without checking out submodules, you can run `git submodule update --init --recursive`. If you don't want to clone complete submodules, you can add `--depth=1` to the update command.

To be able to publish the app in stores e.g. in Google Play its necessary to populate some configs with private keys, etc.
Check `./configure.sh --help` to see how to copy the configs automatically from a private repository.

## Desktop app

### Preparing

You need a Linux or a MacOS machine to build a desktop version of Organic Maps. [Windows](#windows) users can use the [WSL](https://learn.microsoft.com/en-us/windows/wsl/) (Windows Subsystem for Linux) and follow ["Linux or Mac"](#linux-or-mac) steps described below.

### Linux or MacOS

Ensure that you have at least 20GB of free space.

Install Cmake (**3.22.1** minimum), Boost, Qt 6 and other dependencies.

Installing *ccache* can speed up active development.

#### Ubuntu

##### Fully supported versions

_Ubuntu 24.04 or newer:_

```bash
sudo apt update && sudo apt install -y \
    build-essential \
    clang \
    cmake \
    ninja-build \
    python3 \
    qt6-base-dev \
    qt6-positioning-dev \
    libc++-dev \
    libfreetype-dev \
    libglvnd-dev \
    libgl1-mesa-dev \
    libharfbuzz-dev \
    libicu-dev \
    libqt6svg6-dev \
    libqt6positioning6-plugins \
    libqt6positioning6 \
    libsqlite3-dev \
    zlib1g-dev
```

##### Workarounds for older Ubuntu versions

| Software  | Minimum version | Impacted Ubuntu release | Workaround                                                  |
| --------- | --------------- | ----------------------- | ----------------------------------------------------------- |
| CMake     | `3.22.1`        | `20.04` and older       | Install newer `cmake` from [PPA](https://apt.kitware.com/) or from `snap`<br> with `sudo snap install --classic cmake` |
| FreeType  | `2.13.1`        | `22.04` and older       | Install newer `libfreetype6` and `libfreetype-dev` from [PPA](https://launchpad.net/~reviczky/+archive/ubuntu/freetype) |
| GeoClue   | `2.5.7`         | `20.04` and older       | Install newer `geoclue-2.0` from [PPA](https://launchpad.net/~savoury1/+archive/ubuntu/backports) |
| Qt 6      | `6.4.0`         | `22.04` and older       | Build and install Qt 6.4 manually |


```bash
sudo add-apt-repository -y ppa:savoury1/qt-6-2
```

#### Linux Mint

Check which Ubuntu version is the `PACKAGE BASE` for your Linux Mint release [here](https://www.linuxmint.com/download_all.php),
and apply the [Ubuntu workarounds accordingly](#workarounds-for-older-ubuntu-versions).

#### Fedora

```bash
sudo dnf install -y \
    clang \
    cmake \
    ninja-build \
    freetype-devel \
    libicu-devel \
    libstdc++-devel \
    mesa-libGL-devel \
    libglvnd-devel \
    qt6-qtbase-devel \
    qt6-qtpositioning \
    qt6-qtpositioning-devel \
    qt6-qtsvg-devel \
    sqlite-devel
```

#### Alpine

```bash
sudo apk add \
    cmake \
    freetype-dev \
    g++ \
    icu-dev \
    mesa-gl \
    ninja-build \
    qt6-qtbase-dev \
    qt6-qtpositioning-dev \
    qt6-qtsvg-dev \
    samurai \
    sqlite-dev
```

#### macOS

```bash
brew install cmake ninja qt@6
```

### Windows

We haven't compiled Organic Maps on Windows *natively* in a long time, though it is possible.
Some files should be updated. There is a work in progress on [windows](https://github.com/organicmaps/organicmaps/tree/windows) branch.
Please contribute if you have time.
You'll need to have python3, cmake, ninja, and QT6 in the PATH, and Visual Studio 2022 or Visual Studio 2022 Build Tools installed. Use [Visual Studio Developer Command Prompt](https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell?view=vs-2022) or generate Visual Studio project files with CMake to build the project.

However, it is possible to use the WSL (Windows Subsystem for Linux) to run GUI applications.

#### Windows 11 (WSL)

To run Linux GUI apps, you'll need to [install a driver](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps) matching your system. This enables a virtual GPU allowing hardware-accelerated OpenGL rendering.
- [Intel GPU Driver](https://www.intel.com/content/www/us/en/download/19344/intel-graphics-windows-dch-drivers.html)
- [AMD GPU Driver](https://www.amd.com/en/support)
- [NVIDIA GPU DRIVER](https://www.nvidia.com/Download/index.aspx?lang=en-us)

Once a GPU driver is installed and you have [built the app](#building-1) you should be able to [run](#running) it without any additional steps.

#### Windows 10 (WSL)

For Windows 10 you should do these steps (taken from [here](https://techcommunity.microsoft.com/t5/windows-dev-appconsult/running-wsl-gui-apps-on-windows-10/ba-p/1493242), check this blog post if you have any problems):
1. Download and install [VcXsrv Windows X Server](https://sourceforge.net/projects/vcxsrv/).
2. Run _XLaunch_ app to launch X Server. During settings make sure "Disable access control" checkbox is selected.
3. (optionally) Click "Save configuration" and save configuration to some file (for example to _config.xlaunch_). With this you will be able to quickly run the desktop app in the future.
4. When asked about firewall, allow access for both public and private networks.
5. Add this line:
    ```bash
    export DISPLAY=$(ip route|awk '/^default/{print $3}'):0.0
    ```
    to _/etc/bash.bashrc_ file.
6. Restart WSL.

Now when you want to run the desktop app you just need to first launch the X Server on Windows (for example, by running previously saved _config.xlaunch_ file) and then you should be able to [build](#building-1) and [run](#running) the app from WSL.

Running X Server is also required to run `generate_symbols.sh` script when you change icons for [styles](STYLES.md)


### Building

To build a desktop app:
```bash
tools/unix/build_omim.sh -r desktop
```

The output binary will go into `../omim-build-release`.

Check `tools/unix/build_omim.sh -h` for more build options, e.g. to build a debug version.

Besides _desktop_ there are other targets like _generator_tool_, to see a full list execute:

```bash
tools/unix/build_omim.sh -d help
```

#### Build issues

- If you get "not enough memory" errors during builds, you may disable
  [CMake Unity Builds](https://cmake.org/cmake/help/latest/prop_tgt/UNITY_BUILD.html) with `export UNITY_DISABLE=1`
  or by passing `-DUNITY_DISABLE=1` option to `cmake` invocation. Or you can reduce Unity build batch size from
  the default `50` to a lower value (`2`-`16`) with `export UNITY_BUILD_BATCH_SIZE=8`.
  Note that these changes may significantly increase the build time.

### Running

The generated binaries appear in `../omim-build-<buildtype>`.

A desktop app binary is `OMaps`. To run e.g. a release version:

_Linux:_

```bash
../omim-build-release/OMaps
```

_macOS:_

```bash
../omim-build-release/OMaps.app/Contents/MacOS/OMaps
```

### Testing

Compile all unit tests in Debug mode:

```bash
cmake . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target all
```

Run all unit tests:

```bash
cd build
ctest -L "omim_test" --output-on-failure
```

To run a limited set of tests, use `-R <regex>` flag. To exclude some tests, use `-E <regex>` flag:

```bash
cd build
ctest -R "base_tests|coding_tests" --output-on-failure
ctest -L "omim_test" -E "base_tests|coding_tests" --output-on-failure
```

Some tests [are known to be broken](https://github.com/organicmaps/organicmaps/issues?q=is%3Aissue+is%3Aopen+label%3ATests) and disabled on CI.

### Test Coverage

To generate a test coverage report you'll need [gcovr](https://gcovr.com) and gcov tools installed.

Installing gcovr on Linux:
```bash
pip3 install gcovr
```

Installing gcovr on MacOS:
```bash
brew install gcovr
```

Installing gcov on Linux:
```bash
# If you're using GCC compiler
sudo apt-get install cpp

# If you're using Clang compiler
sudo apt-get install llvm
```

Installing gcov on MacOS:
```bash
# If you're using AppleClang compiler it should already be installed

# If you're using Clang compiler
brew install llvm
```

Steps to generate coverage report:

1. Configure cmake with `-DCOVERAGE_REPORT=ON` flag:
   ```bash
   cmake . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
           -DCMAKE_CXX_FLAGS=-g1 -DCOVERAGE_REPORT=ON
   ```
2. Compile unit tests.
3. Run unit tests.
4. Run coverage report generation:
   ```bash
   cd build
   cmake --build . --target omim_coverage
   ```
5. Report can be found in the `build/coverage_report` folder.

### Debug commands

Organic Maps has some "hidden" debug commands that you can trigger by entering them into the search box.

For example you can switch theme which is very useful for checking [styles](STYLES.md) changes.
To switch themes you can enter this commands:
- `?light` - Day theme
- `?dark` - Night theme
- `?vlight` - Day theme for vehicle navigation
- `?vdark` - Night theme for vehicle navigation
- `?olight` - Outdoors day theme
- `?odark` - Outdoors night theme

There are also other commands for turning on/off isolines, anti-aliasing, etc. Check [DEBUG_COMMANDS.md](DEBUG_COMMANDS.md) to learn about them.

### More options

To make the desktop app display maps in a different language add a `-lang` option, e.g. for the Russian language:

```bash
../omim-build-release/OMaps -lang ru
```

By default `OMaps` expects a repository's `data` folder to be present in the current working directory, add a `-data_path` option to override it.

Check `OMaps -help` for a list of all run-time options.

When running the desktop app with lots of maps, increase the open files limit. In MacOS the default value is only 256.
Use `ulimit -n 2000`, put it into `~/.bash_profile` to apply it to all new sessions.
In MacOS to increase this limit globally, add `limit maxfiles 2048 2048` to `/etc/launchd.conf`
and run

```bash
echo 'ulimit -n 2048' | sudo tee -a /etc/profile
```

If you have Qt installed in an unusual directory, use `QT_PATH` variable (`SET (QT_PATH "your-path-to-qt")`). You can skip building tests
with `CMAKE_CONFIG=-DSKIP_TESTS` variable. You would need 1.5 GB of memory
to compile the `stats` module.

The `build_omim.sh` script basically runs these commands:
```bash
    cmake <path_to_omim> -DCMAKE_BUILD_TYPE={Debug|Release}
    <make or ninja> [<target>] -j <number_of_processes>
```

## Android app

### Preparing

Linux, MacOS, or Windows should work to build Organic Maps for Android.

Ensure that you have at least 30GB of free space and Python 3 installed.

Install [Android Studio](https://developer.android.com/studio).

Run Android Studio and open the project in `android/` directory!
This is important, otherwise the following menus won't be visible.

Install Android SDK and NDK:

- Open "SDK Manager" (under "More Actions" in a welcome screen or a three-dot menu in a list of recent projects screen or "Tools" top menu item in an open project).
- Select "Android 15.0 ("Vanilla Ice Cream") / API Level 35" SDK.
- Switch to "SDK Tools" tab.
- Check "Show Package Details" checkbox.
- Select "NDK (Side by side)" version **27.1.12297006**.
- Select "CMake" version **3.22.1**.
- Click "Apply" and wait for downloads and installation to finish.
- In the left pane menu select "Appearance & Behavior > System Settings > Memory Settings".
- Set "IDE max heap size" to 2048MB or more (otherwise the Studio might get stuck on "Updating indexes" when opening the project).

Configure the repository with Android SDK and NDK paths. You can do it either by setting a global environment variable pointing at your Android SDK:

```
ANDROID_HOME=<here is the absolute path to the root folder of your Android SDK installation>
```

or by running the following script, that creates `android/local.properties` file with the line `sdk.dir=<path to your Android SDK>` in it:

_Linux:_

```bash
./tools/android/set_up_android.py --sdk $HOME/Android/Sdk
```

_macOS:_

```bash
./tools/android/set_up_android.py --sdk $HOME/Library/Android/Sdk
```

_Windows 10:_ no action needed, should work out of the box.

### Create a device

Setup a virtual device to use [emulator](https://developer.android.com/studio/run/emulator) ("Tools > Device Manager") or [use a hardware device for debugging](https://developer.android.com/studio/run/device).
If using an emulator, make sure to choose a system image with API Level 34. Use ABI _x86_64_ for Intel-based processors and _arm64-v8a_ for ARM-based processors (e.g. M1/M2 Macs).

Android Studio has issues in parsing the C++ part of the project, please let us know if you know how to resolve it. As a workaround, for working C++ suggestions, you may use:

- [Qt Creator](https://www.qt.io/product/development-tools)
- [Xcode](https://developer.apple.com/xcode/)
- [CLion](https://www.jetbrains.com/clion/)

For Xcode it is required to run `cmake . -g Xcode` to generate project files, while CLion and QT Creator can import CMakeLists.txt.


### Building

There is a matrix of different build variants:

- _Type_:
  - `Debug` is a debug version with all checks enabled.
  - `Beta` is a manual pre-release build for testing.
  - `Release` is a fully optimized version for app stores.

- _Flavor_:
  - `Web` is a light APK without any bundled maps.
  - `Google` is a full Google Play store version including a low-zoom overview world map.
  - `Fdroid` is a version for publishing on the [F-Droid](https://f-droid.org/) open source apps store (no bundled maps and no Google services).
  - ...and other flavors like `Huawei`.

You can choose a build variant in Android Studio's "Build > Select Build Variant..." menu. There you can also choose a target architecture (Active ABI) like _x86_64_ (for e.g. emulator) or _arm64-v8a_ (many modern devices).
In order to build the Google variant, you need a special key which only the core developers have. For community members who want to contribute, the best selection is "fdroidBeta" or "fdroidDebug" depending on the use case.
The Active ABI can be set to "arm64-v8a".

To build and run the app in the emulator or on a hardware device use a "Run > Run (android)" menu item or press the Play / Debug button on the top right of the IDE.

To build a redistributable APK use a "Build > Build Bundle(s) / APK(s) > Build APK(s)" menu item. Generated APKs are stored in `build/outputs/apk/`.

See also https://developer.android.com/studio/run.


### Debugging

To enable logging in case of crashes, after installing a debug version, run:

```bash
adb shell pm grant app.organicmaps.debug android.permission.READ_LOGS
```


### Android Auto Development

Android Auto can be developed and tested without having a physical device by using [Desktop Head Unit (DHU)](https://developer.android.com/training/cars/testing/dhu). Go to Android Studio > Tools -> SDK Manager -> SDK Tools and enable "Android Auto Desktop Head Unit".

[Android Auto App](https://play.google.com/store/apps/details?id=com.google.android.projection.gearhead) is required for Auto functionality. The app should be installed from Google Play before connecting a phone to the Desktop Head Unit or a real car. Android Auto doesn't work on phones without Google Play Services.

To run Android Auto, connect the phone using USB cable and run the Desktop Head Unit with `--usb` flag:

```
~/Library/Android/sdk/extras/google/auto/desktop-head-unit --usb
```

```
[REDACTED]
[I]: Found device 'SAMSUNG SAMSUNG_Android XXXXXXXXX' in accessory mode (vid=18d1, pid=2d01).
[I]: Found accessory: ifnum: 0, rd_ep: 129, wr_ep: 1
[I]: Attaching to USB device...
[I]: Attached!
```

Organic Maps icon will appear in the application list in DHU.

### More options

#### Building from the command line

First configure `PATH` to prefer `cmake` from the Android SDK/NDK instead of the default system install:

_Linux:_

```bash
export PATH=$HOME/Android/Sdk/cmake/3.22.1/bin:$PATH
```

_macOS:_

```bash
export PATH=$HOME/Library/Android/Sdk/cmake/3.22.1/bin:$PATH
```

Check if you have a system-wide Java Runtime Environment (JRE) installed:

```bash
java -version
```

If your system doesn't have a JRE installed or Java version is less than 17 (OpenJDK)
or you want command line builds to use a JRE version bundled with the Studio
then set the `JAVA_HOME` environment variable:

_Linux:_

```bash
export JAVA_HOME=<path-to-android-studio-installation>/android-studio/jre
```

_macOS:_

```bash
export JAVA_HOME=<path-to-android-studio-installation>/Contents/jre/Contents/Home
```

Run the builds from the android subdirectory of the repository:

```bash
cd android
```

To build, install and run e.g. a _Web Debug_ version on your device/emulator:

```bash
./gradlew runWebDebug
```

Or to compile a redistributable _Fdroid Beta_ APK for testing:

```bash
./gradlew assembleFdroidBeta
```

Or to build _Beta_ APKs for all _Flavors_:

```bash
./gradlew assembleBeta
```

Run `./gradlew tasks` to see all possible build variants.

Intermediate files for each build (_Type_ + _Flavor_ + target arch) take ~3-4.5Gb of space.
To remove all intermediate build files run `./gradlew clean`.

By default the build will run for all 3 target architectures: _arm64-v8a_, _armeabi-v7a_ and _x86_64_. To speed up your build include only the arch you need by adding e.g. a `-Parm64` option to the gradle build command (other options are `-Parm32` for _armeabi-v7a_, `-Px64` for _x86_64_ and `-Px86` for _x86_).

To create separate APKs for all target arches add a `-PsplitAPK` option (by default all arches are combined in one "fat" APK).

Adding a `-Ppch` (use precompiled headers) option makes builds ~15% faster.

If building makes your computer slow and laggy, then try lowering the priority of the build process by adding a `--priority=low` option and/or add a `-Pnjobs=<N>` option to limit the number of parallel processes.

See also https://developer.android.com/studio/build/building-cmdline.

To add any of those options to in-studio builds list them in "Command-line Options" in "File > Settings... > Build, Execution, Deployment > Compiler"

#### Reduce resource usage

You can install
[Android SDK](https://developer.android.com/sdk/index.html) and
[NDK](https://developer.android.com/tools/sdk/ndk/index.html) without
Android Studio. Please make sure that SDK for API Level 33,
NDK version **27.1.12297006** and CMake version **3.22.1** are installed.

If you are low on RAM, disk space or traffic there are ways to reduce system requirements:
- exclude the `cpp` folder from indexing. If you do not make any work on the C++ code, this will greatly improve the start-up performance and the ram usage of Android Studio. Click on the `Project` tab on the left, find the `cpp` folder (should be next to the `java` folder), right click on it and select `Mark Directory as` -> `Excluded` (red folder icon). Then restart Android Studio.
- in Android Studio enable "File > Power Save Mode";
- disable the "Android NDK Support" plugin in "Settings -> Plugins" completely and use another IDE (Visual Studio Code, Qt Creator, etc.) for editing C++ code instead;
- don't install Android Studio, run builds and emulator from command line;
- build only for target arches you actually need, e.g. `arm64`;
- for debugging use an older emulated device with low RAM and screen resolution, e.g. "Nexus S";
- make sure the emulator uses [hardware acceleration](https://developer.android.com/studio/run/emulator-acceleration);
- don't use emulator, debug on a hardware device instead.

#### Enable Vulkan Validation

1. Download Vulkan Validation Layers
```bash
./tools/android/download_vulkan_validation_layers.py
```

2. Set `enableVulkanDiagnostics=ON` in `gradle.properties`.

If you build the app from command line, the parameter can be passed via command line.

E.g.
```
./gradlew -Parm64 -PenableVulkanDiagnostics=ON runGoogleDebug
```

## iOS app

### Preparing

Building Organic Maps for iOS requires a Mac.

Ensure that you have at least 20GB of free space.

After [getting all sources](#getting-sources), please make sure that Command Line Tools are installed:

```bash
xcode-select --install
```

Then, install [Xcode](https://apps.apple.com/app/xcode/id497799835?mt=12) from the App Store.

Enroll in the [Apple Developer Program](https://developer.apple.com/programs/) (you can run Organic Maps in Simulator without this step).

### Configuring Xcode

Set up your developer account and add certificates:

- Open Xcode.
- Click "Xcode" → "Preferences".
- Open "Account" tab.
- Enter account credentials from the previous step.
- Click "Manage Certificates".
- Click "+" and choose "Apple Development".
- You may also need to register your Mac in your Apple Developer account.

Reconfigure the project to use your developer signing keys:

- Open `xcode/omim.xcworkspace` in Xcode.
- Click on "Maps" project.
- Open "Signing & Capabilities" tab.
- Choose a unique bundle identifier (not app.organicmaps.debug) and your team.
- Select "Automatically manage signing".

If you want to run Organic Maps on a real device, you have to remove the CarPlay entitlement. Open `iphone/Maps/OMaps-Debug.entitlements`
and remove the `com.apple.developer.carplay-maps` entry. Now you can sign your app again in the "Signing & Capabilities" tab. Testing CarPlay
on a real device requires [requesting entitlements from Apple](https://developer.apple.com/documentation/carplay/requesting_carplay_entitlements).

### Building and running

Open `xcode/omim.xcworkspace` in Xcode.

Select "OMaps" product scheme.

- Choose "Your Mac (Designed for iPad)" to run on Mac without using Simulator.
- Choose either "iPhone _" or "iPad _" to run in the Simulator.

Compile and run the project ("Product" → "Run").

## Map data and styles
See readme for the [map generator](https://github.com/organicmaps/organicmaps/blob/master/docs/MAPS.md) and [styles](https://github.com/organicmaps/organicmaps/blob/master/docs/STYLES.md) if you need to customize the map files and styles

# Available debug commands

Organic Maps exposes debug commands to help you control the native components (engine, editor, navigation, ...). **These commands are not intended for regular users** and are only used for debug purposes. Please only use these commands if you are working on Organic Maps.

Each command is entered in the search input (Android and iOS) and are activated as soon as the full search keyword is entered. Unless specified, the effects triggered are discarded after a restart.

For more information, please see the source code at [`Framework::ParseSearchQueryCommand`](../map/framework.cpp).

## Drape

### Themes

- `?dark` or `mapstyle:dark`: Enable night mode for the map view only. You may need to change the zoom level to reload the view.
- `?light` or `mapstyle:light`: Enable light mode for the map view only. You may need to change the zoom level to reload the view.
- `?odark` or `mapstyle:outdoors_dark`: Same as `?dark`, but using the outdoor variant.
- `?olight` or `mapstyle:outdoors_light`: Same as `?light`, but using the outdoor variant.
- `?vdark` or `mapstyle:vdark`: Same as `?dark`, but using the vehicle variant.
- `?vlight` or `mapstyle:vlight`: Same as `?light`, but using the vehicle variant.

### Post processing

- `?aa` or `effect:antialiasing`: Enable antialiasing.
- `?no-aa` or `effect:no-antialiasing`: Disable antialiasing.

### Map layers

- `?scheme`: Enable the subway layer.
- `?no-scheme`: Disable the subway layer.
- `?isolines`: Enable the isolines layer.
- `?no-isolines`: Disable the isolines layer.

### 3D mode (for the Qt desktop app only)
- `?3d`: Enable 3D (perspective) mode.
- `?b3d`: Enable 3D buildings.
- `?2d`: Disable 3D mode and buildings.

The source code is at [`SearchPanel::Try3dModeCmd`](../qt/search_panel.cpp).

### Information

- `?debug-info`: Show renderer version, zoom scale and FPS counter in the top left corner of the map.
- `?debug-info-always`: Same as `?debug-info`, but persists across restarts.
- `?no-debug-info`: Disables the debug info.
- `?debug-rect`: Shows boxes around icons and labels. When the icon/label is shown, the box is green. When the icon/label cannot be shown, the box is red with a blue arrow indicating which icon/label prevents rendering. When the icon/label is not ready for display, the box is yellow (check the `Update` method of [`dp::OverlayHandle`](../drape/overlay_handle.hpp) and derived classes for more information).
- `?no-debug-rect`: Disables the debug boxes.

### Drape rendering engine

All the following commands require an app restart:

- `?gl`: Forces the OpenGL renderer. OpenGL renderer is supported on all platforms and is used by default on older Android devices and on the desktop.
- `?vulkan`: Forces the Vulkan renderer on Android. Vulkan is used by default on newer Android 7+ devices.
- `?metal`: Forces the Metal renderer. It is used by default on iOS and can be ported/enabled with some effort on Mac OS X too.

## Editor

- `?edits`: Shows the list of map edits (local and uploaded) made with the app in the search results. Useful to debug what was not uploaded yet.
- `?eclear`: Clears the locally stored list of edits. Warning: all local changes that are not yet uploaded to OpenStreetMap.org will be lost! Everything that was already uploaded to OSM stays there untouched.

## Routing

- `?debug-cam`: Force-enables speed cameras in all countries.
- `?no-debug-cam`: Reverts speed camera setting to default.

## GPS

- `?gpstrackaccuracy:XXX`: Changes the accuracy of the GPS while recording tracks. Replace `XXX` by the desired horizontal accuracy. Works only on iOS for now.

## Place Page

- `?all-types`: Shows all internal types in place page
- `?no-all-types`: Disables showing all internal types in place page

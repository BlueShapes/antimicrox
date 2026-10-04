# <img src="./src/images/antimicrox.png" alt="Icon" width="60"/> AntiMicroX-Delta

1. [Description](#description)  
2. [License](#license)  
3. [Installation](#installation)
4. [Wiki](#wiki)
5. [Command Line](#command-line) 
6. [D-Bus](#d-bus)
7. [Testing Under Linux](#testing-under-linux)
8. [AntiMicroX Profiles](#antimicrox-profiles)
9. [Support](#support)

## Project identity

AntiMicroX-Delta is an independently maintained GPLv3 derivative based on
upstream AntiMicroX 3.6.1. It uses its own version series, package identifiers,
settings directory, IPC names, and release channel. It is not an official
release of the upstream AntiMicroX project.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for the complete Delta release history and the
upstream history retained for attribution.

## Description

AntiMicroX-Delta is a graphical program used to map gamepad keys to keyboard, mouse, scripts and macros. You can use this program to control any desktop application with a gamepad on Linux🐧 and Windows 🪟.
It can be also used for generating SDL2 configuration (useful for mapping atypical gamepads to generic ones like xbox360).

We support X.org and Wayland.

Features:

- Mapping of gamepads/joystick buttons to:
  - keyboard buttons
  - mouse buttons and moves
  - scripts and executables
  - macros consisting of elements mentioned above
- Assigning multiple switchable sets of mappings to gamepad.
- Auto profiles - assign profile to active application window (not in Wayland [note](https://github.com/AntiMicroX/antimicrox/issues/303)).
- Windows integration with Controllable Delta: automatically pause mapped output
  from all controllers while the mod can read a controller and its Minecraft
  window is in the foreground. Enable or disable it in General settings.
  Both applications need builds containing the bridge. See
  [the integration protocol and verification notes](CONTROLLABLE_DELTA.md).

This program is currently supported under various Linux
distributions.

This application is continuation of project called `AntiMicro`, which was later abandoned and revived by juliagoda.

Legacy repositories:
- First AntiMicroX repository: https://github.com/juliagoda/antimicroX
- Second, maintained by organization: https://github.com/AntiMicro/antimicro
- First, original AntiMicro repository: https://github.com/Ryochan7/antimicro

**Screenshots:**  
Disclaimer: Theme may depend on your system configuration.

<table border="0px" >
  <tr>
    <td>
      <img src="./other/appdata/screenshots/app_light.png" alt="Main Window" />
    </td>
    <td>
      <img src="./other/appdata/screenshots/controllermapping.png" alt="Mapping" />
    </td>
  </tr>
  <tr>
    <td>
      <img src="./other/appdata/screenshots/calibration.png" alt="Calibration" />
    </td>
    <td>
    <img src="./other/appdata/screenshots/advanced.png" alt="Advanced settings" />
    </td>
  </tr>
</table>

## License

This program is licensed under the GPL v.3. Please read the LICENSE text document
included with the source code if you would like to read the terms of the license.
The license can also be found online at
http://www.gnu.org/licenses/gpl.txt

### Fork notice

This repository contains a modified version of AntiMicroX. Existing copyright,
authorship, license, and warranty notices are preserved. AntiMicroX-Delta is
distributed under GNU GPL version 3 or later and is not endorsed by the upstream
AntiMicroX maintainers. See [LICENSE](LICENSE) and [CHANGELOG.md](CHANGELOG.md).

## Installation

### Windows

Download `antimicrox-delta-X.X.X-Windows-AMD64.exe` from the
[AntiMicroX-Delta release page](https://github.com/BlueShapes/antimicrox/releases/latest) and install it.

If AntiMicroX-Delta terminates because of an unhandled Windows exception, a diagnostic
minidump is saved under `%LOCALAPPDATA%\antimicrox-delta\crashes` (or `crashes` beside
a locally built portable executable). At most five AntiMicroX-Delta dumps are retained. Minidumps
can contain paths, stack data, and fragments of in-memory input, so inspect them
before sharing them publicly.

### Flatpak

AntiMicroX-Delta does not currently publish an official Flathub package. The
upstream `io.github.antimicrox.antimicrox` package is a separate application and
does not receive Delta updates.

❕ Flatpak package may not work correctly with wayland [(Fix available here)](https://github.com/AntiMicroX/antimicrox/wiki/Open-uinput-error)

### AppImage

Download from the [AntiMicroX-Delta release site](https://github.com/BlueShapes/antimicrox/releases).

It is recommended to use [AppImageLauncher](https://github.com/TheAssassin/AppImageLauncher) with this package.

### Debian/Ubuntu-based distributions

Download from the [AntiMicroX-Delta release site](https://github.com/BlueShapes/antimicrox/releases) and install the `.deb` package.

### Upstream distribution packages (not AntiMicroX-Delta)

The Fedora, openSUSE, Arch, and third-party package-manager commands below
install upstream AntiMicroX. They are retained as upstream documentation and do
not install or update AntiMicroX-Delta.

#### Fedora

```
dnf install antimicrox
```

#### openSUSE

A [package](https://software.opensuse.org/package/antimicrox) is available.

```
zypper install antimicrox
```

#### Arch Linux or Arch Linux based distributions

```
trizen -S antimicrox
```
**or**

pre-built version can de downloaded from unofficial repository called [chaotic-aur](https://lonewolf.pedrohlc.com/chaotic-aur/).

Append (one of listed mirrors) to `/etc/pacman.conf`:
```bash
# Brazil
Server = http://lonewolf-builder.duckdns.org/$repo/$arch
# Germany
Server = http://chaotic.bangl.de/$repo/$arch
# USA (Cloudflare cached)
Server = https://repo.kitsuna.net/$arch
# Netherlands
Server = https://chaotic.tn.dedyn.io/$arch
```
To check signature, add keys:
```bash
sudo pacman-key --keyserver hkp://keyserver.ubuntu.com -r 3056513887B78AEB 8A9E14A07010F7E3
sudo pacman-key --lsign-key 3056513887B78AEB
sudo pacman-key --lsign-key 8A9E14A07010F7E3
```
Install package
```bash
pacman -S antimicrox
```

### Building Yourself

List of required dependencies and build instructions can be found [here](./BUILDING.md).

### Upstream package status

Status of package `antimicrox`:  
[![Packaging status](https://repology.org/badge/vertical-allrepos/antimicrox.svg?columns=3&minversion=3.1)](https://repology.org/project/antimicrox/versions)

## Command Line

Run `antimicrox-delta --help` or read `man antimicrox-delta` for command-line parameters.

<details>
  <summary>Commandline for flatpak</summary>
  The separate upstream Flatpak package can be launched with:
  <br>
  <code>flatpak run io.github.antimicrox.antimicrox</code> instead of just <code>antimicrox</code>
  <br>
  In some cases it may be good to add alias
  <br>
  <code>alias antimicrox='flatpak run io.github.antimicrox.antimicrox'</code><br>
  fo file <code>~/.bashrc</code>
</details>

## D-Bus

AntiMicroX-Delta provides the D-Bus service
`io.github.blueshapes.AntiMicroXDelta`. You can use it to control some aspects
of AntiMicroX-Delta, such as selecting the current control set.

For example, to select set 0 for input device 0 with dbus-send:

```
dbus-send --print-reply --dest=io.github.blueshapes.AntiMicroXDelta /io/github/blueshapes/AntiMicroXDelta/inputdevice/0 io.github.blueshapes.AntiMicroXDelta.InputDevice.setActiveSetNumber int32:0
```

### Objects

AntiMicroX-Delta provides InputDevice objects with paths
`/io/github/blueshapes/AntiMicroXDelta/inputdevice/<N>`, where
`<N>` is the device index.

To find a device of interest, enumerate those objects and use `getSDLName` and
`getDescription` to identify the device.

### Interfaces

InputDevice objects support the
[`io.github.blueshapes.AntiMicroXDelta.InputDevice`](other/io.github.antimicrox.inputdevice.xml)
interface.

#### Method: io.github.blueshapes.AntiMicroXDelta.InputDevice.getSDLName()

`getSDLName()` provides the human-readable name of the device, such as "Microsoft Xbox 360 Controller" or "HORIPAD FPS for Nintendo Switch".

#### Method: io.github.blueshapes.AntiMicroXDelta.InputDevice.getDescription()

`getDescription()` provides a detailed description of the device:

```
Index:            1
  UniqueID:         030081b85e0400008e020000100100001118654
  GUID:             030081b85e0400008e02000010010000
  VendorID:         1118
  ProductID:        654
  Serial:
  Product Version:  272
  Name:             Xbox 360 Controller
  Game Controller: Yes
  # of RawAxes:    6
  # of Axes:       6
  # of RawButtons: 21
  # of Buttons:    21
  # of Hats:       0
  Accelerometer:   0
  Gyroscope:       0
```

This includes:

* The controller's `UniqueID` assigned by AntiMicroX-Delta
* The controller's `GUID` assigned by SDL
* The controller's USB `VendorID`, `ProductID`, `Serial`, `ProductVersion`, and
  `Name`
* Whether the device is a `Game Controller`
* The controller's input features: `# of RawAxes`, `# of Axes`,
  `# of RawButtons`, `# of Buttons`, `# of Hats`, `Accelerometer`, and
  `Gysroscope`

#### Method: io.github.blueshapes.AntiMicroXDelta.InputDevice.getActiveSetNumber()

`getActiveSetNumber()` returns the current set number for this device.

API set indices are 0-based, but they are displayed in the UI with 1-based
labels.

#### Method: io.github.blueshapes.AntiMicroXDelta.InputDevice.getActiveSetName()

`getActiveSetName()` returns the name of the current set for this device.

This is empty if the set was not given a name. In that case, AntiMicroX-Delta
displays a default name: `Set <N>` with a 1-based index.

#### Method: io.github.blueshapes.AntiMicroXDelta.InputDevice.setActiveSetNumber()

`setActiveSetNumber()` changes the active set for this device to the set
specified, as a 0-based index.

### Test

Use [D-Spy](https://gitlab.gnome.org/GNOME/d-spy) to inspect and test the D-Bus
interface.

## Wiki

[Look here](https://github.com/BlueShapes/antimicrox/wiki)

## Testing Under Linux

If you are having problems with AntiMicroX-Delta detecting a controller or
detecting all axes and buttons, you should test the controller outside of
AntiMicroX-Delta to check whether the problem is in AntiMicroX-Delta. The two endorsed
programs for testing gamepads outside of AntiMicroX-Delta are **sdl-jstest**
(**sdl2-jstest**) and **evtest**. SDL2 utilizes evdev on Linux so performing
testing with older programs that use joydev won't be as helpful since some
devices behave a bit differently between the two systems. Another method also exists, 
which can be found [here](https://github.com/juliagoda/SDL_JoystickButtonNames).

## AntiMicroX Profiles

If you would like to send the profile you are using for your application or find something 
for yourself, [here](https://github.com/AntiMicroX/antimicrox-profiles) is the forked repository. If you want to report a bug, ask 
a question or share a suggestion about that collection, use the
[antimicrox-profiles](https://github.com/AntiMicroX/antimicrox-profiles) page.

## Support

For AntiMicroX-Delta support, use the [BlueShapes issue tracker](https://github.com/BlueShapes/antimicrox/issues).
Upstream AntiMicroX issues and translation infrastructure remain separate.

### Contributing

Any contributions into codebase are welcome. You can find contribution guide [here](./CONTRIBUTING.md).  
Some issues are may have bounties which are meant to attract contributors.

### Translation

AntiMicroX-Delta currently inherits upstream translations. Their translation
process is handled through [Weblate](https://hosted.weblate.org/engage/antimicrox).

Translation status

<a href="https://hosted.weblate.org/engage/antimicrox/">
<img src="https://hosted.weblate.org/widgets/antimicrox/-/gui/multi-auto.svg" alt="Translation status" />
</a>

More information about translating can be found [here](https://github.com/AntiMicroX/antimicrox/wiki/Translating-AntiMicroX).

# DuckStation-UWP - a PSX emulator, ported to UWP
[Features](#features) | [Downloading and Running](#downloading-and-running) | [Building](#building) | [Getting Help](#getting-help) | [Disclaimers](#disclaimers)

**Game Compatibility List:** https://docs.google.com/spreadsheets/d/e/2PACX-1vRE0jjiK_aldpICoy5kVQlpk2f81Vo6P4p9vfg4d7YoTOoDlH4PQHoXjTD2F7SdN8SSBLoEAItaIqQo/pubhtml

**Wiki:** https://www.duckstation.org/wiki/

DuckStation is an simulator/emulator of the Sony PlayStation(TM) console, focusing on playability, speed, and long-term maintainability. The goal is to be as accurate as possible while maintaining performance suitable for low-end devices. "Hack" options are discouraged, the default configuration should support all playable games with only some of the enhancements having compatibility issues.

DuckStation-UWP is a modification of said emulator to reintroduce the long discontinued Xbox/UWP port of the emulator, authored by irixaligned. The goal is to keep as up to date as possible with DuckStation, simply adding, rather than subtracting or removing, and while keeping as close as possible, both in functionality and ease of use, to the PC version of the emulator.

A "BIOS" ROM image is required to to start the emulator and to play games. You can use an image from any hardware version or region, although mismatching game regions and BIOS regions may have compatibility issues. A ROM image is not provided with the emulator for legal reasons, you should dump this from your own console using Caetla or other means.

## Features

DuckStation-UWP uses DuckStation's "Big Picture Mode"/TV UI, built in Dear ImGui.

Other features include:

 - CPU Recompiler/JIT (x86-64, armv7/AArch32, AArch64, RISC-V/RV64. Only x86-64 is actively maintained by this port, but you can place your bets anyways if you feel lucky)
 - Hardware rendering with D3D11/D3D12 and software rendering.
 - Upscaling, texture filtering, and true colour (24-bit) in hardware renderers.
 - PGXP for geometry precision, texture correction, and depth buffer emulation.
 - Adaptive downsampling filter.
 - Post processing shader chains (GLSL and experimental Reshade FX. Neither currently function under Xbox due to it's requirement of HLSL shaders. This is being looked into.)
 - "Fast boot" for skipping BIOS splash/intro.
 - Save state support.
 - Xbox and Windows UWP support. (Win32, macOS, and Linux are not guaranteed to work with this codebase, and are not the focus of the project.)
 - Supports bin/cue images, raw bin/img files, MAME CHD, single-track ECM, MDS/MDF, and unencrypted PBP formats.
 - Direct booting of homebrew executables.
 - Direct loading of Portable Sound Format (psf) files.
 - Digital and analog controllers for input (rumble is forwarded to host).
 - Namco GunCon lightgun support (simulated with mouse).
 - NeGcon support.
 - "Big Picture" UI.
 - Automatic updates with preview and latest channels.
 - Automatic content scanning - game titles/hashes are provided by redump.org.
 - Optional automatic switching of memory cards for each game.
 - Supports loading cheats from existing lists.
 - Memory card editor and save importer.
 - Emulated CPU overclocking.
 - Integrated and remote debugging.
 - Multitap controllers (up to 8 devices).
 - RetroAchievements.
 - Automatic loading/applying of PPF patches.

## System Requirements
 - A CPU faster than a potato. But it needs to be x86_64, AArch32/armv7, AArch64/ARMv8, or RISC-V/RV64.
 - For the hardware renderers, a GPU capable of OpenGL 3.1/OpenGL ES 3.1/Direct3D 11 Feature Level 10.0 (or Vulkan 1.0) and above. So, basically anything made in the last 10 years or so.
 - An XInput compatible game controller (e.g. XB360/XBOne/XBSeries). Users on Windows using other controllers (like the DualShock/DualSense) may need to use a translation layer.

## Downloading and running
Binaries of DuckStation for Windows x64/ARM64, Linux x86_64 (in AppImage/Flatpak formats), and macOS Universal Binaries are available via DuckStation's GitHub Releases and are automatically built with every commit/push to their repo.

DuckStation-UWP binaries for Windows (UWP, x64) and Xbox are available in the releases section and have a general release timeframe of every 3 days to 2 weeks. If you want the latest and greatest changes ASAP, you can build from source.

### Xbox
Xbox is only supported in Dev Mode -- retail is NOT supported, no questions asked.

To download:
- Go to https://github.com/irixaligned/duckstation-uwp/releases/latest. Download the provided .appx file and Dependencies.zip.
- Extract Dependencies.zip wherever you want.

To install:
- Open your Xbox Device Portal, navigate to Home > My games & apps.
- Hit "Add".
- Where it says "Install packaged application", either hit browse and select the .appx file, or drag it into the area that says "or drop package file here", then hit Next.
- When it asks you to choose necessary dependencies, either select or drag in the .appx file(s) in Dependencies/x64 from where you extracted Dependencies.zip.
- Press "Start" and wait for it to finish.
- Afterwards, navigate to your Xbox and enter Dev Home.
- Hit View on DuckStation-UWP, then hit "Show Details".
- Set the application type from "App" to "Game".
- Launch it and play!

Contrary to most Xbox homebrew, DuckStation-UWP typically requires the dependencies to run. If you get an error "0x80070002" upon starting the application, you forgot the dependencies. Uninstall and try again.

### Windows (UWP)

Enable developer mode, and install the AppX package with the Windows package installer or the `Add-AppxPackage` PowerShell command.

Due to the certificate being self signed, you may not be able to install the package due to not trusting my certificate. Do the following:
- Right click on the package
- Go to Properties
- Go to Digital Signatures
- Select the signature belonging to "irixaligned" from the list
- Click Details, then View Certificate
- Click Install Certificate
- Select Local Machine as the store location
- Select "Place all certificates in the following store"
- Install it to the "Trusted People" certificate store
- If you are still unable to install the package, do the above, except select "Trusted Root Certification Authorities" as the store. Do note that this is a large security risk and should be done only as a last resort.

### LibCrypt protection and SBI files

A number of PAL region games use LibCrypt protection, requiring additional CD subchannel information to run properly. libcrypt not functioning usually manifests as hanging or crashing, but can sometimes affect gameplay too, depending on how the game implemented it.

For these games, make sure that the CD image and its corresponding SBI (.sbi) file have the same name and are placed in the same directory. DuckStation will automatically load the SBI file when it is found next to the CD image.

For example, if your disc image was named `Spyro3.cue`, you would place the SBI file in the same directory, and name it `Spyro3.sbi`.

CHD images with built-in subchannel information are also supported.

## Building

### UWP
Requirements:
 - Visual Studio 2022

1. Clone the repository: `git clone https://github.com/irixaligned/duckstation-uwp.git`.
2. Download the dependencies pack from https://github.com/stenzek/duckstation-ext-qt-minimal/releases/download/latest/deps-x64.7z, and extract it to `dep\msvc`.
3. Open the Visual Studio solution `duckstation.sln` in the root.
4. Right click the `duckstation-uwp` project and then click Publish > Create App Packages and follow the instructions in the wizard. Make sure to build target ReleaseUWP, otherwise you will get compiler errors.

A signature is required to install appx files on Xbox. Generate a self-signed certificate when asked for one.

## User Directories
The "User Directory" is where you should place your BIOS images, where settings are saved to, and memory cards/save states are saved by default.
An optional [SDL game controller database file](#sdl-game-controller-database) can be also placed here.

This is located in the following places depending on the platform you're using:

- Windows: My Documents\DuckStation
- Linux: `$XDG_DATA_HOME/duckstation`, or `~/.local/share/duckstation`.
- macOS: `~/Library/Application Support/DuckStation`.

On Xbox, this is located in DuckStation-UWP's folder in LocalAppData, within the LocalState subdirectory.

## Default bindings
DuckStation-UWP's default bindings are intended for Xbox. These mappings are to the first controller detected upon starting DuckStation-UWP.

Controller 1:
 - **Sticks, D-Pad**: Respective Xbox Sticks & D-Pad
 - **Cross/Circle/Square/Triangle:** A/B/X/Y
 - **L1/R1:** LB/RB
 - **L2/R2:** LT/RT
 - **L3/R3:** LS/RS
 - **Select:** View
 - **Start:** Menu
 - **Analog Toggle (for DualShock):** View + Menu

Hotkeys:
 - **Open Pause Menu:** LS + RS

## Getting help
Support for this project, alongside new release announcements, and generally most things related to Xbox emulation, are provided by the Emulation Hub discord.

- Discord: https://discord.gg/xboxemus
- Dev Store: https://xboxdevstore.github.io

## Disclaimers

Icon by icons8: https://icons8.com/icon/74847/platforms.undefined.short-title

Primary work on DuckStation is done by Stenzek. This is just a port of his emulator back to the Xbox and other UWP platforms. You can find his repo here: https://github.com/stenzek/duckstation

"PlayStation" and "PSX" are registered trademarks of Sony Interactive Entertainment Europe Limited. This project is not affiliated in any way with Sony Interactive Entertainment.

If you obtained any specialized version of this emulator (e.g. through Patreon, Retail mode, or a special Discord release), paid for this software in any fashion, or obtained any other sort of publicized release without appropriately matching source code, please contact me immediately in the event this has happened. This software is FREE and is not monetized by me in any fashion.

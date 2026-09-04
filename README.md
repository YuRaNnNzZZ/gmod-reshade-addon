# gmod-reshade

ReShade add-on for Garry's Mod that injects a Lua function to draw effects at will.

It also by default suppresses ReShade's automatic effect pass at the end of the current frame. This can be toggled in the ReShade's add-on settings.

Combined with the bundled Lua addon, it will make ReShade effects draw in the [RenderScreenspaceEffects hook](https://wiki.facepunch.com/gmod/GM:RenderScreenspaceEffects), before any UI is drawn.

## Installation

1. Download the add-on from latest release, unpack `bin` to your `steamapps/GarrysMod` folder. Make sure the `gmod_reshade.addon32` or `gmod_reshade.addon64` is next to your ReShade DLL (`d3d9.dll`/`dxgi.dll`)
1. Subscribe to the [integration addon from Steam Workshop](https://steamcommunity.com/sharedfiles/filedetails/?id=3795768174).
	- Or, alternatively, download it from the latest release and unpack it to your `addons` folder.

## Usage

Accompanied [integration addon](addons/pp_reshade/) handles ReShade compatibility automatically.

How to toggle: `Spawnmenu` -> `Post Process` -> `Shaders` -> `ReShade` (enabled by default) or use the `pp_reshade` convar.

If, for some reason, you need to draw them manually, use the `render.DrawReShadeEffects()` function. It is not present in the very first Lua state (autorun) so make sure to check for it's existence first (example is in the addon linked above.)

## Build

The repository contains ReShade and `gmod-module-base` as submodules. Configure and build each architecture with Visual Studio:

```powershell
git submodule update --init --recursive

cmake -S . -B build-x64 -A x64
cmake --build build-x64 --config Release

cmake -S . -B build-x86 -A Win32
cmake --build build-x86 --config Release
```

The resulting files are `gmod_reshade.addon64` and `gmod_reshade.addon32`. Copy the file matching the Garry's Mod architecture next to the ReShade DLL.

## Disclaimer

AI/LLM is used for creation of this project: the C++ code and GitHub Actions workflow.

Lua code is written by me, because no LLM can come up with this schizo-tier code.

The binaries are build by GitHub Actions and published to releases automatically.
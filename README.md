# gmod-reshade

ReShade add-on for Garry's Mod that injects `render.DrawReShadeEffects()` into the client Lua state.

The function renders all currently enabled ReShade effects at the point where Lua calls it. ReShade's `effect_runtime::render_effects` also marks the effects as rendered for the current frame, so the normal presentation pass does not render them again over the game UI.

## Build

The repository contains ReShade and `gmod-module-base` as submodules. Configure and build each architecture with Visual Studio:

```powershell
cmake -S . -B build-x64 -A x64
cmake --build build-x64 --config Release

cmake -S . -B build-x86 -A Win32
cmake --build build-x86 --config Release
```

The resulting files are `gmod_reshade.addon64` and `gmod_reshade.addon32`. Copy the file matching the Garry's Mod architecture next to the ReShade DLL.

## Usage

Call the function from a client render hook before drawing UI that should stay unaffected by ReShade:

```lua
hook.Add("RenderScreenspaceEffects", "DrawReShadeEffectsBeforeUI", function()
    if render.DrawReShadeEffects then
        render.DrawReShadeEffects()
    end
end)
```

The guard covers the first startup frame if the client Lua state is created after ReShade initializes its effect runtime. The add-on preserves the graphics state tracked by the ReShade API after rendering the effects. On D3D11.1 it additionally preserves constant-buffer ranges set through `*SetConstantBuffers1`, which ReShade 6.8's internal state block otherwise resets. Its state tracker uses an add-on-specific private-data identifier, so it can coexist with add-ons such as ReShade Effect Shader Toggler that include their own copy of ReShade's state-tracking utility. Calling the function more than once during one frame is safe because ReShade ignores subsequent calls until the next presentation.

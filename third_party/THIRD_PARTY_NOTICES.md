# Third-Party Notices

## Dear ImGui 1.93.0

Copyright (c) 2014-2025 Omar Cornut
Licensed under the MIT License. See `imgui/LICENSE.txt`.

## ImPlot 1.1

Copyright (c) 2020 Evan Pezent
Licensed under the MIT License. See `implot/LICENSE`.

## Liquid glass shader reference

The HLSL glass shader in `src/render/Shaders.h` is a Direct3D 11 port of the
shader and math modules from `imgui-liquid-glass-main`, which is adapted from
AndroidLiquidGlassView (c) 2025 QmDeve, licensed under the MIT License.

Source reference:
- `src/ui/widget/effects/glass/shaders/opengl/liquid_glass.frag`
- `src/ui/widget/effects/glass/shaders/common/*.glslinc`

The desktop port keeps the same algorithm (rounded-rect SDF, edge refraction,
RGB dispersion, tint filter, rim lighting and outer shadow) and replaces the
GLES/Vulkan plumbing with a Direct3D 11 implementation.

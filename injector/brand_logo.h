// ============================================================================
//  Passive - injector brand mark.
//
//  The standalone loader shows the same official logo as the in-game menu. The
//  PNG is decoded with Windows Imaging Component and uploaded as an immutable
//  D3D11 shader resource once, beside the exe (or in assets/).
// ============================================================================
#pragma once
#include <imgui.h>
#include <d3d11.h>

namespace tyr {
namespace injector {

// Creates (once) and returns the logo SRV, or nullptr when the file is missing.
ImTextureID logoTexture(ID3D11Device* device);

} // namespace injector
} // namespace tyr

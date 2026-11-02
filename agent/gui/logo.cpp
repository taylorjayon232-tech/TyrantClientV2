// ============================================================================
//  Passive - brand mark implementation.
// ============================================================================
#include "logo.h"
#include "../render/glex.h"   // windows.h + GL loader (must come first)

#include <wincodec.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace tyr {
namespace gui {

namespace {

ImTextureID g_texture = 0;
bool g_resolved = false;

std::wstring moduleDirectory() {
    HMODULE module = nullptr;
    wchar_t path[MAX_PATH] = L"";
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&moduleDirectory), &module))
        return {};
    if (!GetModuleFileNameW(module, path, MAX_PATH)) return {};
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return {};
    *slash = 0;
    return path;
}

std::wstring executableDirectory() {
    wchar_t path[MAX_PATH] = L"";
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return {};
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return {};
    *slash = 0;
    return path;
}

// One decoder pass: WIC -> 32-bit RGBA, rows flipped for GL's bottom-up upload.
bool decode(const std::wstring& path, std::vector<unsigned char>& rgba, int& width, int& height) {
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    bool ok = false;

    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = SUCCEEDED(com);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) return false;

    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory))) &&
        SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                     WICDecodeMetadataCacheOnLoad, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame)) &&
        SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
        SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                                        WICBitmapDitherTypeNone, nullptr, 0.0,
                                        WICBitmapPaletteTypeCustom))) {
        UINT w = 0, h = 0;
        if (SUCCEEDED(frame->GetSize(&w, &h)) && w > 0 && h > 0 && w <= 2048 && h <= 2048) {
            const UINT stride = w * 4;
            std::vector<unsigned char> pixels((size_t)stride * h);
            if (SUCCEEDED(converter->CopyPixels(nullptr, stride, (UINT)pixels.size(), pixels.data()))) {
                rgba.resize(pixels.size());
                for (UINT y = 0; y < h; ++y)
                    std::memcpy(rgba.data() + (size_t)y * stride,
                                pixels.data() + (size_t)(h - 1 - y) * stride, stride);
                width = (int)w;
                height = (int)h;
                ok = true;
            }
        }
    }

    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    if (uninit) CoUninitialize();
    return ok;
}

ImTextureID upload(const std::vector<unsigned char>& rgba, int width, int height) {
    if (!tyr::glx::g_glexReady || width <= 0 || height <= 0) return 0;
    if (rgba.size() < (size_t)width * (size_t)height * 4) return 0;
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (!texture) return 0;
    GLint previous = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, previous);
    return (ImTextureID)(ImU64)texture;
}

} // namespace

ImTextureID logoTexture() {
    if (g_resolved) return g_texture;
    if (!tyr::glx::g_glexReady) return 0;   // retry on the next frame
    g_resolved = true;

    const std::wstring module = moduleDirectory();
    const std::wstring exe = executableDirectory();
    std::vector<std::wstring> candidates;
    if (!module.empty()) {
        candidates.push_back(module + L"\\assets\\passive-logo.png");
        candidates.push_back(module + L"\\passive-logo.png");
    }
    if (!exe.empty() && exe != module) {
        candidates.push_back(exe + L"\\passive-logo.png");
        candidates.push_back(exe + L"\\assets\\passive-logo.png");
    }
    candidates.push_back(L"assets\\passive-logo.png");
    candidates.push_back(L"passive-logo.png");

    for (const std::wstring& path : candidates) {
        std::vector<unsigned char> rgba;
        int width = 0, height = 0;
        if (!decode(path, rgba, width, height)) continue;
        g_texture = upload(rgba, width, height);
        if (g_texture) return g_texture;
    }
    return 0;
}

bool logoResolved() { return g_resolved; }

bool drawLogo(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 tint, float opacity) {
    const ImTextureID texture = logoTexture();
    if (!texture || !dl) return false;
    const ImU32 color = (tint & ~IM_COL32_A_MASK) |
        ((ImU32)(((tint >> IM_COL32_A_SHIFT) & 0xFF) * std::clamp(opacity, 0.f, 1.f)) << IM_COL32_A_SHIFT);
    dl->AddImage(texture, p0, p1, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), color);
    return true;
}

} // namespace gui
} // namespace tyr

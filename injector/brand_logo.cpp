// ============================================================================
//  Passive - injector brand mark implementation.
// ============================================================================
#include "brand_logo.h"

#include <windows.h>
#include <wincodec.h>
#include <cstring>
#include <string>
#include <vector>

namespace tyr {
namespace injector {

namespace {

ImTextureID g_texture = 0;
bool g_resolved = false;

std::wstring executableDirectory() {
    wchar_t path[MAX_PATH] = L"";
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return {};
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return {};
    *slash = 0;
    return path;
}

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
                // D3D also wants the first row at the top, which is the order WIC gives.
                rgba = std::move(pixels);
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

ImTextureID upload(ID3D11Device* device, const std::vector<unsigned char>& rgba,
                   int width, int height) {
    if (!device || width <= 0 || height <= 0) return 0;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = (UINT)width;
    desc.Height = (UINT)height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = rgba.data();
    data.SysMemPitch = (UINT)width * 4;
    ID3D11Texture2D* texture = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, &data, &texture)) || !texture) return 0;

    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = desc.Format;
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    ID3D11ShaderResourceView* srv = nullptr;
    const HRESULT hr = device->CreateShaderResourceView(texture, &view, &srv);
    texture->Release();
    if (FAILED(hr) || !srv) return 0;
    return reinterpret_cast<ImTextureID>(srv);
}

} // namespace

ImTextureID logoTexture(ID3D11Device* device) {
    if (g_resolved) return g_texture;
    if (!device) return 0;
    g_resolved = true;

    const std::wstring exe = executableDirectory();
    std::vector<std::wstring> candidates;
    if (!exe.empty()) {
        candidates.push_back(exe + L"\\passive-logo.png");
        candidates.push_back(exe + L"\\assets\\passive-logo.png");
    }
    candidates.push_back(L"assets\\passive-logo.png");
    candidates.push_back(L"passive-logo.png");

    for (const std::wstring& path : candidates) {
        std::vector<unsigned char> rgba;
        int width = 0, height = 0;
        if (!decode(path, rgba, width, height)) continue;
        g_texture = upload(device, rgba, width, height);
        if (g_texture) return g_texture;
    }
    return 0;
}

} // namespace injector
} // namespace tyr

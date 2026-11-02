// ============================================================================
//  Tyrant — standalone test for agent/render/png_image.h
//
//  Exercises the decoder against real PNG files produced by an independent
//  implementation (.NET, via tools/png_testdata.ps1). The raw DEFLATE stream is
//  supplied from disk so the test does not need the JVM's Inflater: it covers
//  the part we wrote — chunk parsing, unfiltering (Sub/Up/Average/Paeth), the
//  colour-type conversions and the GL row flip.
//
//  Not part of the CMake build (only agent/*.cpp is compiled).
//
//  Build + run:
//    powershell -NoProfile -ExecutionPolicy Bypass -File tools\png_testdata.ps1
//    cl /nologo /std:c++20 /EHsc /W4 /Fe:.pngtest\pngtest.exe /Fo:.pngtest\ tools\png_decode_test.cpp
//    .pngtest\pngtest.exe
// ============================================================================
#include "../agent/render/png_image.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::vector<unsigned char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// The expect files hold one AARRGGBB line per pixel, top row first.
bool readExpected(const std::string& path, std::vector<unsigned int>& out) {
    std::ifstream file(path);
    if (!file) return false;
    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
        if (line.size() < 8) continue;
        unsigned int value = 0;
        for (int i = 0; i < 8; ++i) {
            const int digit = hexValue(line[line.size() - 8 + (size_t)i]);
            if (digit < 0) return false;
            value = (value << 4) | (unsigned int)digit;
        }
        out.push_back(value);
    }
    return !out.empty();
}

bool runCase(const std::string& dir, const std::string& name) {
    const std::string base = dir + "/" + name;
    const std::vector<unsigned char> png = readFile(base + ".png");
    const std::vector<unsigned char> raw = readFile(base + ".raw");
    std::vector<unsigned int> expected;
    if (png.empty() || raw.empty() || !readExpected(base + ".expect", expected)) {
        std::printf("FAIL %s: missing test data (run tools/png_testdata.ps1)\n", name.c_str());
        return false;
    }

    tyr::pngimg::Parsed parsed;
    if (!tyr::pngimg::parseChunks(png.data(), png.size(), parsed)) {
        std::printf("FAIL %s: parseChunks rejected the file\n", name.c_str());
        return false;
    }
    tyr::pngimg::Image image;
    if (!tyr::pngimg::toRgba(parsed, raw, image, /*flipVertical=*/true)) {
        std::printf("FAIL %s: toRgba rejected the data\n", name.c_str());
        return false;
    }

    if ((size_t)image.width * (size_t)image.height != expected.size()) {
        std::printf("FAIL %s: %dx%d gives %zu pixels, expected %zu\n", name.c_str(),
                    image.width, image.height,
                    (size_t)image.width * (size_t)image.height, expected.size());
        return false;
    }

    int mismatches = 0;
    for (int y = 0; y < image.height; ++y) {
        // Our rows run bottom-first, the expectation file is top-first.
        const int expectRow = image.height - 1 - y;
        for (int x = 0; x < image.width; ++x) {
            const unsigned char* px = &image.rgba[((size_t)y * (size_t)image.width + (size_t)x) * 4];
            const unsigned int got = ((unsigned int)px[3] << 24) | ((unsigned int)px[0] << 16) |
                                     ((unsigned int)px[1] << 8) | (unsigned int)px[2];
            const unsigned int want = expected[(size_t)expectRow * (size_t)image.width + (size_t)x];
            if (got != want) {
                if (mismatches < 5)
                    std::printf("     pixel (%d,%d): got %08X want %08X\n", x, expectRow, got, want);
                ++mismatches;
            }
        }
    }

    if (mismatches) {
        std::printf("FAIL %s: %d of %zu pixels differ (colorType %d, bitDepth %d, %dx%d)\n",
                    name.c_str(), mismatches, expected.size(), parsed.colorType,
                    parsed.bitDepth, parsed.width, parsed.height);
        return false;
    }
    std::printf("PASS %s: %dx%d colorType %d bitDepth %d, %zu pixels match .NET\n",
                name.c_str(), image.width, image.height, parsed.colorType,
                parsed.bitDepth, expected.size());
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".pngtest";
    bool ok = true;
    ok = runCase(dir, "rgba") && ok;
    ok = runCase(dir, "palette") && ok;
    std::printf("%s\n", ok ? "ALL PNG DECODE TESTS PASSED" : "PNG DECODE TESTS FAILED");
    return ok ? 0 : 1;
}

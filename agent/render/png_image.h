// ============================================================================
//  Passive — minimal, dependency-free PNG reader (header only).
//
//  Vanilla item textures are ordinary PNGs, so the overlay only needs the
//  narrow slice of the format they use: 1/2/4/8-bit greyscale, RGB, palette,
//  greyscale+alpha and RGBA, non-interlaced. Decompression is supplied by the
//  caller (the JVM's own java.util.zip.Inflater, part of java.base), so this
//  file contains no zlib of its own and nothing to go wrong at run time: an
//  unsupported or corrupt image simply reports false and the caller keeps its
//  text fallback.
//
//  Kept header-only and free of JNI/GL so it can be exercised by a standalone
//  decoder test with real PNG files.
// ============================================================================
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace tyr {
namespace pngimg {

struct Parsed {
    int width = 0;
    int height = 0;
    int bitDepth = 8;
    int colorType = 6;
    int interlace = 0;
    bool ok = false;
    std::vector<unsigned char> idat;      // raw zlib stream (all IDAT chunks)
    std::vector<unsigned char> palette;   // PLTE entries, RGB triplets
    std::vector<unsigned char> trns;      // tRNS payload, meaning depends on colour type
};

struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;      // width*height*4, row 0 first (top row)
};

inline uint32_t readBe32(const unsigned char* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// Chunk walk: IHDR / PLTE / tRNS / IDAT / IEND. Anything malformed stops the
// walk and reports whatever was already found as unusable.
inline bool parseChunks(const unsigned char* data, size_t size, Parsed& out) {
    out = Parsed{};
    static const unsigned char kSignature[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
    if (!data || size < 8 || std::memcmp(data, kSignature, 8) != 0) return false;

    bool seenHeader = false;
    size_t offset = 8;
    while (offset + 12 <= size) {
        const uint32_t length = readBe32(data + offset);
        const unsigned char* type = data + offset + 4;
        const size_t payloadAt = offset + 8;
        if (length > size || payloadAt + length > size) return false;   // truncated
        const unsigned char* payload = data + payloadAt;

        if (std::memcmp(type, "IHDR", 4) == 0) {
            if (length < 13) return false;
            out.width = (int)readBe32(payload);
            out.height = (int)readBe32(payload + 4);
            out.bitDepth = payload[8];
            out.colorType = payload[9];
            const int compression = payload[10];
            const int filter = payload[11];
            out.interlace = payload[12];
            seenHeader = true;
            if (compression != 0 || filter != 0) return false;
            if (out.width <= 0 || out.height <= 0 || out.width > 4096 || out.height > 4096) return false;
            if (out.interlace != 0) return false;
            if (out.bitDepth != 1 && out.bitDepth != 2 && out.bitDepth != 4 && out.bitDepth != 8) return false;
            if (out.colorType != 0 && out.colorType != 2 && out.colorType != 3 &&
                out.colorType != 4 && out.colorType != 6) return false;
            // Sub-byte depths only exist for greyscale and palette images.
            if (out.bitDepth != 8 && out.colorType != 0 && out.colorType != 3) return false;
        } else if (std::memcmp(type, "PLTE", 4) == 0) {
            out.palette.assign(payload, payload + length);
        } else if (std::memcmp(type, "tRNS", 4) == 0) {
            out.trns.assign(payload, payload + length);
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            if (!seenHeader) return false;
            out.idat.insert(out.idat.end(), payload, payload + length);
            if (out.idat.size() > (size_t)64 * 1024 * 1024) return false;
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        offset = payloadAt + length + 4;    // skip the CRC
    }

    if (!seenHeader || out.idat.empty()) return false;
    if (out.colorType == 3 && out.palette.empty()) return false;
    out.ok = true;
    return true;
}

inline int paethPredictor(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = p > a ? p - a : a - p;
    const int pb = p > b ? p - b : b - p;
    const int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

// Unfilters the inflated IDAT stream and converts it to 8-bit RGBA.
// `flipVertical` puts row 0 last, which is what an OpenGL upload wants.
inline bool toRgba(const Parsed& p, const std::vector<unsigned char>& raw, Image& out,
                   bool flipVertical) {
    if (!p.ok) return false;

    const int channels = p.colorType == 0 ? 1 : p.colorType == 2 ? 3 :
                         p.colorType == 3 ? 1 : p.colorType == 4 ? 2 : 4;
    const int bitsPerPixel = channels * p.bitDepth;
    const size_t filterOffset = (size_t)std::max(1, (bitsPerPixel + 7) / 8);
    const size_t rowBytes = ((size_t)p.width * (size_t)bitsPerPixel + 7) / 8;
    const size_t scanline = rowBytes + 1;
    const size_t needed = scanline * (size_t)p.height;
    if (raw.size() < needed) return false;

    out.width = p.width;
    out.height = p.height;
    out.rgba.assign((size_t)p.width * (size_t)p.height * 4, 0);

    std::vector<unsigned char> current(rowBytes, 0);
    std::vector<unsigned char> previous(rowBytes, 0);
    const size_t paletteEntries = p.palette.size() / 3;

    for (int y = 0; y < p.height; ++y) {
        const unsigned char* line = raw.data() + scanline * (size_t)y;
        const int filter = line[0];
        if (filter > 4) return false;
        std::memcpy(current.data(), line + 1, rowBytes);

        for (size_t i = 0; i < rowBytes; ++i) {
            const int a = (i >= filterOffset) ? current[i - filterOffset] : 0;
            const int b = y > 0 ? previous[i] : 0;
            const int c = (y > 0 && i >= filterOffset) ? previous[i - filterOffset] : 0;
            int value = current[i];
            switch (filter) {
                case 1: value += a; break;
                case 2: value += b; break;
                case 3: value += (a + b) / 2; break;
                case 4: value += paethPredictor(a, b, c); break;
                default: break;
            }
            current[i] = (unsigned char)(value & 0xff);
        }

        unsigned char* dstRow = out.rgba.data() +
            (size_t)(flipVertical ? (p.height - 1 - y) : y) * (size_t)p.width * 4;
        size_t bitPos = 0;
        for (int x = 0; x < p.width; ++x) {
            int samples[4] = { 0, 0, 0, 0 };
            for (int ch = 0; ch < channels; ++ch) {
                if (p.bitDepth == 8) {
                    const size_t at = (size_t)x * (size_t)channels + (size_t)ch;
                    samples[ch] = at < rowBytes ? current[at] : 0;
                } else {
                    int value = 0;
                    for (int bit = 0; bit < p.bitDepth; ++bit) {
                        const size_t byteIndex = bitPos >> 3;
                        const int shift = 7 - (int)(bitPos & 7u);
                        const int bitValue = byteIndex < rowBytes ? ((current[byteIndex] >> shift) & 1) : 0;
                        value = (value << 1) | bitValue;
                        ++bitPos;
                    }
                    samples[ch] = value;
                }
            }

            unsigned char* px = dstRow + (size_t)x * 4;
            if (p.colorType == 6) {
                px[0] = (unsigned char)samples[0];
                px[1] = (unsigned char)samples[1];
                px[2] = (unsigned char)samples[2];
                px[3] = (unsigned char)samples[3];
            } else if (p.colorType == 2) {
                px[0] = (unsigned char)samples[0];
                px[1] = (unsigned char)samples[1];
                px[2] = (unsigned char)samples[2];
                px[3] = 255;
                if (p.trns.size() >= 6 && samples[0] == p.trns[1] &&
                    samples[1] == p.trns[3] && samples[2] == p.trns[5]) px[3] = 0;
            } else if (p.colorType == 4) {
                px[0] = px[1] = px[2] = (unsigned char)samples[0];
                px[3] = (unsigned char)samples[1];
            } else if (p.colorType == 0) {
                // Scale a sub-byte greyscale sample up to 0-255.
                const int maxValue = (1 << p.bitDepth) - 1;
                const int grey = p.bitDepth == 8 ? samples[0]
                                                 : (samples[0] * 255) / (maxValue ? maxValue : 1);
                px[0] = px[1] = px[2] = (unsigned char)grey;
                px[3] = 255;
                if (p.trns.size() >= 2 && samples[0] == p.trns[1]) px[3] = 0;
            } else {   // palette
                const size_t index = (size_t)samples[0];
                if (index >= paletteEntries) return false;
                px[0] = p.palette[index * 3 + 0];
                px[1] = p.palette[index * 3 + 1];
                px[2] = p.palette[index * 3 + 2];
                px[3] = index < p.trns.size() ? p.trns[index] : 255;
            }
        }
        previous.swap(current);
    }
    return true;
}

} // namespace pngimg
} // namespace tyr

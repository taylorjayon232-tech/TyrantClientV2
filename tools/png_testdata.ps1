# ============================================================================
#  Tyrant — test data for tools/png_decode_test.cpp
#
#  Generates real PNGs with .NET's encoder, then dumps two things for the C++
#  decoder test:
#    <name>.png     the image itself
#    <name>.raw     the raw DEFLATE stream (IDAT with the 2-byte zlib header
#                   stripped), so the test can exercise unfiltering without a
#                   JVM Inflater
#    <name>.expect  one "AARRGGBB" line per pixel, top row first, read back from
#                   the encoded file by .NET's own decoder
#
#  Usage:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\png_testdata.ps1
# ============================================================================
Add-Type -AssemblyName System.Drawing

$outDir = Join-Path (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)) ".pngtest"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# Expectations come from re-decoding the saved file, so they describe what an
# independent implementation actually reads out of the PNG.
function Save-Expect([string]$pngPath, [string]$base) {
    $decoded = New-Object System.Drawing.Bitmap($pngPath)
    $sb = New-Object System.Text.StringBuilder
    for ($y = 0; $y -lt $decoded.Height; $y++) {
        for ($x = 0; $x -lt $decoded.Width; $x++) {
            [void]$sb.AppendLine($decoded.GetPixel($x, $y).ToArgb().ToString("X8"))
        }
    }
    $decoded.Dispose()
    [System.IO.File]::WriteAllText("$base.expect", $sb.ToString())
}

function Save-RawDeflate([string]$pngPath, [string]$base) {
    $bytes = [System.IO.File]::ReadAllBytes($pngPath)
    $idat = New-Object System.IO.MemoryStream
    $offset = 8
    while ($offset + 12 -le $bytes.Length) {
        $len = [int]$bytes[$offset] * 16777216 + [int]$bytes[$offset+1] * 65536 +
               [int]$bytes[$offset+2] * 256 + [int]$bytes[$offset+3]
        $type = [System.Text.Encoding]::ASCII.GetString($bytes, $offset + 4, 4)
        if ($type -eq "IDAT") { $idat.Write($bytes, $offset + 8, $len) }
        if ($type -eq "IEND") { break }
        $offset += 12 + $len
    }
    $zlib = $idat.ToArray()
    # Skip the 2-byte zlib header; DeflateStream reads raw DEFLATE and stops at
    # the final block, so the trailing adler32 never matters.
    $mem = New-Object System.IO.MemoryStream(, $zlib[2..($zlib.Length - 1)])
    $inflate = New-Object System.IO.Compression.DeflateStream($mem, [System.IO.Compression.CompressionMode]::Decompress)
    $raw = New-Object System.IO.MemoryStream
    $inflate.CopyTo($raw)
    $inflate.Dispose()
    [System.IO.File]::WriteAllBytes("$base.raw", $raw.ToArray())
}

# ---- 1) RGBA8, odd size, wide value range, varying alpha -------------------
$w = 7; $h = 5
$bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
        $a = 255 - (($x * 7 + $y * 11) % 96)
        $r = ($x * 37) % 256
        $g = ($y * 53) % 256
        $b = (($x + $y) * 29) % 256
        $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($a, $r, $g, $b))
    }
}
$base = Join-Path $outDir "rgba"
$bmp.Save("$base.png", [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Save-Expect "$base.png" "$base"
Save-RawDeflate "$base.png" "$base"

# ---- 2) 8-bit palette, 64 distinct colours ---------------------------------
# SetPixel is unsupported on indexed bitmaps, so the indices go in through
# LockBits. This exercises the palette index -> RGB path (pixel alpha is covered
# by the RGBA case above; palette tRNS is not exercised by this generator).
$w = 16; $h = 9
$idx = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format8bppIndexed)
$pal = $idx.Palette
for ($i = 0; $i -lt 64; $i++) {
    $pal.Entries[$i] = [System.Drawing.Color]::FromArgb(
        255, ($i % 4) * 85, ([math]::Floor($i / 4) % 4) * 85, ([math]::Floor($i / 16) % 4) * 85)
}
$idx.Palette = $pal

$rect = New-Object System.Drawing.Rectangle(0, 0, $w, $h)
$locked = $idx.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly,
                        [System.Drawing.Imaging.PixelFormat]::Format8bppIndexed)
$stride = $locked.Stride
$pixels = New-Object byte[] ($stride * $h)
for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
        $pixels[$y * $stride + $x] = [byte](($x * 5 + $y * 17) % 64)
    }
}
[System.Runtime.InteropServices.Marshal]::Copy($pixels, 0, $locked.Scan0, $pixels.Length)
$idx.UnlockBits($locked)

$base = Join-Path $outDir "palette"
$idx.Save("$base.png", [System.Drawing.Imaging.ImageFormat]::Png)
$idx.Dispose()
Save-Expect "$base.png" "$base"
Save-RawDeflate "$base.png" "$base"

Write-Host "wrote test data to $outDir"
Get-ChildItem $outDir | Where-Object { $_.Name -notlike "*.bat" } | Select-Object -ExpandProperty Name

param(
    [string]$Source = (Join-Path $PSScriptRoot '..\assets\gate-monitor-source.png'),
    [string]$Destination = (Join-Path $PSScriptRoot '..\assets\gate-monitor.ico')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$sizes = @(16, 20, 24, 32, 48, 64, 128, 256)
$sourceImage = [System.Drawing.Image]::FromFile((Resolve-Path -LiteralPath $Source))
$images = [System.Collections.Generic.List[byte[]]]::new()
try {
    foreach ($size in $sizes) {
        $bitmap = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.DrawImage($sourceImage, 0, 0, $size, $size)
            } finally {
                $graphics.Dispose()
            }
            for ($y = 0; $y -lt $size; $y++) {
                for ($x = 0; $x -lt $size; $x++) {
                    $color = $bitmap.GetPixel($x, $y)
                    if ($color.A -eq 0) { continue }
                    if ($size -le 32 -and $x -gt (0.66 * $size) -and $y -lt (0.35 * $size)) {
                        $bitmap.SetPixel($x, $y, [System.Drawing.Color]::Transparent)
                        continue
                    }
                    if ($color.R -gt 220 -and $color.G -gt 220 -and $color.B -gt 220) {
                        $flat = [System.Drawing.Color]::FromArgb($color.A, 255, 255, 255)
                    } elseif ($color.R -gt 170 -and $color.G -lt 170 -and $color.B -lt 100) {
                        $flat = [System.Drawing.Color]::FromArgb($color.A, 255, 112, 0)
                    } elseif ($color.G -gt 110 -and $color.B -gt 110 -and $color.B -gt $color.R) {
                        $flat = [System.Drawing.Color]::FromArgb($color.A, 0, 205, 220)
                    } else {
                        $flat = [System.Drawing.Color]::FromArgb($color.A, 31, 42, 54)
                    }
                    $bitmap.SetPixel($x, $y, $flat)
                }
            }
            $stream = [System.IO.MemoryStream]::new()
            try {
                $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
                $images.Add($stream.ToArray())
            } finally {
                $stream.Dispose()
            }
        } finally {
            $bitmap.Dispose()
        }
    }
} finally {
    $sourceImage.Dispose()
}

$output = [System.IO.MemoryStream]::new()
$writer = [System.IO.BinaryWriter]::new($output)
try {
    $writer.Write([uint16]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]$images.Count)
    $offset = 6 + 16 * $images.Count
    for ($index = 0; $index -lt $images.Count; $index++) {
        $size = $sizes[$index]
        $writer.Write([byte]$(if ($size -eq 256) { 0 } else { $size }))
        $writer.Write([byte]$(if ($size -eq 256) { 0 } else { $size }))
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]32)
        $writer.Write([uint32]$images[$index].Length)
        $writer.Write([uint32]$offset)
        $offset += $images[$index].Length
    }
    foreach ($image in $images) {
        $writer.Write($image)
    }
    [System.IO.File]::WriteAllBytes($Destination, $output.ToArray())
} finally {
    $writer.Dispose()
    $output.Dispose()
}

Write-Host "Generated $Destination"

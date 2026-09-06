param([string]$Destination = (Join-Path $PSScriptRoot '..\assets\stat-wisp.ico'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
# Match the simple vector mark in assets/stat-wisp.svg at each native icon size.
$sizes = @(16, 20, 24, 32, 48, 64, 128, 256)
$images = [System.Collections.Generic.List[byte[]]]::new()
foreach ($size in $sizes) {
    $bitmap = [Drawing.Bitmap]::new($size, $size)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(23, 39, 53))
    $pen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(70, 220, 191), 22)
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $stream = [IO.MemoryStream]::new()
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.ScaleTransform($size / 256.0, $size / 256.0)
        $path.AddArc(8,8,64,64,180,90)
        $path.AddArc(184,8,64,64,270,90)
        $path.AddArc(184,184,64,64,0,90)
        $path.AddArc(8,184,64,64,90,90)
        $path.CloseFigure()
        $graphics.FillPath($brush,$path)
        $pen.StartCap = $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
        $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
        $points = [Drawing.PointF[]]@([Drawing.PointF]::new(48,80),[Drawing.PointF]::new(80,176),[Drawing.PointF]::new(128,116),[Drawing.PointF]::new(176,176),[Drawing.PointF]::new(208,80))
        $graphics.DrawLines($pen,$points)
        $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
        $images.Add($stream.ToArray())
        if ($size -eq 256) { $bitmap.Save((Join-Path $PSScriptRoot '..\assets\stat-wisp-source.png'),[Drawing.Imaging.ImageFormat]::Png) }
    } finally {
        $stream.Dispose(); $path.Dispose(); $pen.Dispose(); $brush.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
    }
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

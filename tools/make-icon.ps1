param(
    [Parameter(Mandatory = $true)]
    [string]$OutPath
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

function New-RoundedRectPath {
    param(
        [float]$X,
        [float]$Y,
        [float]$Width,
        [float]$Height,
        [float]$Radius
    )

    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $diameter = $Radius * 2
    $path.AddArc($X, $Y, $diameter, $diameter, 180, 90)
    $path.AddArc($X + $Width - $diameter, $Y, $diameter, $diameter, 270, 90)
    $path.AddArc($X + $Width - $diameter, $Y + $Height - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc($X, $Y + $Height - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()
    return $path
}

function New-CameraBitmap {
    param([int]$Size)

    $bitmap = New-Object System.Drawing.Bitmap $Size, $Size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.Clear([System.Drawing.Color]::Transparent)

    $scale = $Size / 256.0
    $body = New-RoundedRectPath (28 * $scale) (72 * $scale) (200 * $scale) (142 * $scale) (30 * $scale)
    $top = New-RoundedRectPath (70 * $scale) (48 * $scale) (68 * $scale) (44 * $scale) (14 * $scale)

    $shadowBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(52, 0, 0, 0))
    $gradientRect = New-Object System.Drawing.RectangleF (28 * $scale), (48 * $scale), (200 * $scale), (168 * $scale)
    $bodyBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush -ArgumentList `
        $gradientRect, `
        ([System.Drawing.Color]::FromArgb(255, 255, 210, 74)), `
        ([System.Drawing.Color]::FromArgb(255, 245, 149, 45)), `
        ([System.Drawing.Drawing2D.LinearGradientMode]::Vertical)
    $outlinePen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 94, 64, 34)), ([Math]::Max(1, 8 * $scale))
    $lensOuter = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 54, 75, 95))
    $lensMid = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 48, 144, 190))
    $lensInner = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 21, 45, 65))
    $highlight = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(235, 255, 255, 255))
    $flashBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(245, 255, 245, 170))

    $graphics.FillEllipse($shadowBrush, 42 * $scale, 194 * $scale, 172 * $scale, 26 * $scale)
    $graphics.FillPath($bodyBrush, $top)
    $graphics.FillPath($bodyBrush, $body)
    $graphics.DrawPath($outlinePen, $top)
    $graphics.DrawPath($outlinePen, $body)
    $graphics.FillEllipse($lensOuter, 76 * $scale, 92 * $scale, 104 * $scale, 104 * $scale)
    $graphics.FillEllipse($lensMid, 91 * $scale, 107 * $scale, 74 * $scale, 74 * $scale)
    $graphics.FillEllipse($lensInner, 108 * $scale, 124 * $scale, 40 * $scale, 40 * $scale)
    $graphics.FillEllipse($highlight, 112 * $scale, 113 * $scale, 18 * $scale, 18 * $scale)
    $graphics.FillEllipse($flashBrush, 184 * $scale, 90 * $scale, 24 * $scale, 18 * $scale)

    $shadowBrush.Dispose()
    $bodyBrush.Dispose()
    $outlinePen.Dispose()
    $lensOuter.Dispose()
    $lensMid.Dispose()
    $lensInner.Dispose()
    $highlight.Dispose()
    $flashBrush.Dispose()
    $body.Dispose()
    $top.Dispose()
    $graphics.Dispose()

    return $bitmap
}

function Write-UInt16 {
    param([System.IO.BinaryWriter]$Writer, [int]$Value)
    $Writer.Write([byte]($Value -band 0xff))
    $Writer.Write([byte](($Value -shr 8) -band 0xff))
}

function Write-UInt32 {
    param([System.IO.BinaryWriter]$Writer, [int64]$Value)
    $Writer.Write([byte]($Value -band 0xff))
    $Writer.Write([byte](($Value -shr 8) -band 0xff))
    $Writer.Write([byte](($Value -shr 16) -band 0xff))
    $Writer.Write([byte](($Value -shr 24) -band 0xff))
}

function New-IconImageBytes {
    param([System.Drawing.Bitmap]$Bitmap)

    $stream = New-Object System.IO.MemoryStream
    $Bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    return ,$stream.ToArray()
}

$sizes = @(256, 64, 48, 32, 16)
$images = New-Object System.Collections.Generic.List[object]
foreach ($size in $sizes) {
    $bitmap = New-CameraBitmap $size
    [byte[]]$bytes = New-IconImageBytes $bitmap
    $images.Add([pscustomobject]@{
        Size = $size
        Bytes = $bytes
    }) | Out-Null
    $bitmap.Dispose()
}

$directory = Split-Path -Parent $OutPath
if ($directory) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

$file = [System.IO.File]::Open($OutPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
$writer = New-Object System.IO.BinaryWriter $file
try {
    Write-UInt16 $writer 0
    Write-UInt16 $writer 1
    Write-UInt16 $writer $images.Count
    $offset = 6 + 16 * $images.Count
    foreach ($image in $images) {
        $dimensionByte = if ($image.Size -eq 256) { 0 } else { $image.Size }
        $writer.Write([byte]$dimensionByte)
        $writer.Write([byte]$dimensionByte)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        Write-UInt16 $writer 1
        Write-UInt16 $writer 32
        Write-UInt32 $writer $image.Bytes.Length
        Write-UInt32 $writer $offset
        $offset += $image.Bytes.Length
    }
    foreach ($image in $images) {
        $writer.Write($image.Bytes)
    }
}
finally {
    $writer.Dispose()
    $file.Dispose()
}

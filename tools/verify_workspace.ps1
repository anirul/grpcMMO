param(
    [string]$FrameRoot = (Join-Path $PSScriptRoot "..\\external\\frame"),
    [string]$DataRoot = (Join-Path $PSScriptRoot "..\\..\\grpcMMO-data"),
    [switch]$RequireFrame,
    [switch]$RequireData
)

$ErrorActionPreference = "Stop"

function Resolve-WorkspacePath {
    param([string]$PathValue)

    $resolved = Resolve-Path -LiteralPath $PathValue -ErrorAction SilentlyContinue
    if ($null -eq $resolved) {
        return [pscustomobject]@{
            Exists = $false
            Path = [System.IO.Path]::GetFullPath($PathValue)
        }
    }

    return [pscustomobject]@{
        Exists = $true
        Path = $resolved.Path
    }
}

$frame = Resolve-WorkspacePath -PathValue $FrameRoot
$data = Resolve-WorkspacePath -PathValue $DataRoot

$frameHeader = Join-Path $frame.Path "frame\\api.h"
$frameCMake = Join-Path $frame.Path "CMakeLists.txt"
$dataReadme = Join-Path $data.Path "README.md"
$dataAttributes = Join-Path $data.Path ".gitattributes"

$frameReady = $frame.Exists -and (Test-Path -LiteralPath $frameHeader) -and (Test-Path -LiteralPath $frameCMake)
$dataReady = $data.Exists -and (Test-Path -LiteralPath $dataReadme) -and (Test-Path -LiteralPath $dataAttributes)

Write-Host ("Frame root: {0}" -f $frame.Path)
Write-Host ("Frame import ready: {0}" -f $frameReady)
Write-Host ("Data root: {0}" -f $data.Path)
Write-Host ("Data import ready: {0}" -f $dataReady)

if ($RequireFrame -and -not $frameReady) {
    Write-Error "Frame workspace dependency is missing or incomplete."
}

if ($RequireData -and -not $dataReady) {
    Write-Error "grpcMMO-data workspace dependency is missing or incomplete."
}

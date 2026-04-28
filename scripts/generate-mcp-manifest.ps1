Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir

function Read-RawFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return [System.IO.File]::ReadAllText($Path).TrimEnd("`r", "`n")
}

function Read-ToolsBody {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $raw = Read-RawFile -Path $Path
    $lines = $raw -split "`r?`n"
    if ($lines.Length -eq 0 -or $lines[0].Trim() -ne "tools:") {
        throw "Invalid tools fragment: $Path"
    }
    if ($lines.Length -eq 1) {
        return ""
    }
    return ($lines[1..($lines.Length - 1)] -join "`n")
}

$metaPath = Join-Path $repoRoot "mcp_manifest\00-meta.yaml"
$hardwarePath = Join-Path $repoRoot "mcp_manifest\10-hardware.yaml"
$motorPath = Join-Path $repoRoot "mcp_manifest\20-motor.yaml"
$connectionPath = Join-Path $repoRoot "mcp_manifest\30-connection.yaml"
$motionToolsPath = Join-Path $repoRoot "mcp_manifest\40-tools-motion.yaml"
$configToolsPath = Join-Path $repoRoot "mcp_manifest\41-tools-config.yaml"
$actionToolsPath = Join-Path $repoRoot "mcp_manifest\42-tools-actions.yaml"
$httpPath = Join-Path $repoRoot "mcp_manifest\50-http-endpoints.yaml"
$outputPath = Join-Path $repoRoot "ohmni_robot_mcp.yaml"

$parts = @(
    "# ============================================================",
    "# OhmniRobot - MCP Server Manifest",
    "# GENERATED FILE. Edit files under mcp_manifest/ instead.",
    "# Regenerate with: powershell -ExecutionPolicy Bypass -File scripts/generate-mcp-manifest.ps1",
    "# ============================================================",
    "",
    (Read-RawFile -Path $metaPath),
    "",
    "# -- Hardware --------------------------------------------------",
    (Read-RawFile -Path $hardwarePath),
    "",
    "# -- Motor config ----------------------------------------------",
    (Read-RawFile -Path $motorPath),
    "",
    "# -- Connection ------------------------------------------------",
    (Read-RawFile -Path $connectionPath),
    "",
    "# -- MCP Tools -------------------------------------------------",
    "tools:",
    (Read-ToolsBody -Path $motionToolsPath),
    "",
    (Read-ToolsBody -Path $configToolsPath),
    "",
    (Read-ToolsBody -Path $actionToolsPath),
    "",
    "# -- HTTP debug endpoints -------------------------------------",
    (Read-RawFile -Path $httpPath),
    ""
)

$content = $parts -join "`n"
[System.IO.File]::WriteAllText($outputPath, $content + "`n", [System.Text.UTF8Encoding]::new($false))
Write-Output "Generated $outputPath"

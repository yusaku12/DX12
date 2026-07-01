param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectName,
    [Parameter(Mandatory = $true)]
    [string]$SolutionDir
)

$ErrorActionPreference = 'Stop'

$SolutionDir = $SolutionDir.Trim('"')
if ($SolutionDir.EndsWith('\\')) {
    $SolutionDir = $SolutionDir.Substring(0, $SolutionDir.Length - 1)
}

function Get-ModuleNameFromProjectName {
    param([string]$Name)
    if ($Name -eq 'DX12.ThirdParty.ImGui') { return $null }
    if ($Name -notlike 'DX12.*') { return $null }
    return $Name.Substring(5)
}

function Get-SourceFolderForModule {
    param([string]$Module)
    if ([string]::IsNullOrWhiteSpace($Module)) { return $null }
    return Join-Path (Join-Path $SolutionDir 'Source') $Module
}

function Test-GraphHasCycle {
    param([hashtable]$Rules)

    $visiting = New-Object 'System.Collections.Generic.HashSet[string]'
    $visited = New-Object 'System.Collections.Generic.HashSet[string]'

    function Visit {
        param([string]$node, [hashtable]$Rules, $visiting, $visited)

        if ($visited.Contains($node)) { return $false }
        if ($visiting.Contains($node)) { return $true }

        [void]$visiting.Add($node)
        if ($Rules.ContainsKey($node)) {
            foreach ($next in $Rules[$node]) {
                if (Visit -node $next -Rules $Rules -visiting $visiting -visited $visited) {
                    return $true
                }
            }
        }
        [void]$visiting.Remove($node)
        [void]$visited.Add($node)
        return $false
    }

    foreach ($node in $Rules.Keys) {
        if (Visit -node $node -Rules $Rules -visiting $visiting -visited $visited) {
            return $true
        }
    }
    return $false
}

$rulesPath = Join-Path $SolutionDir 'Tool\ModuleDependencyRules.json'
if (-not (Test-Path $rulesPath)) {
    throw "Dependency rule file not found: $rulesPath"
}

$rulesObj = Get-Content $rulesPath -Raw | ConvertFrom-Json
$rulesRaw = @{}
foreach ($p in $rulesObj.PSObject.Properties) {
    $arr = @()
    foreach ($v in $p.Value) { $arr += [string]$v }
    $rulesRaw[$p.Name] = $arr
}
if (Test-GraphHasCycle -Rules $rulesRaw) {
    throw 'Module dependency rules contain a cycle. Please fix Tool/ModuleDependencyRules.json.'
}

$module = Get-ModuleNameFromProjectName -Name $ProjectName
if ($null -eq $module) {
    exit 0
}

if (-not $rulesRaw.ContainsKey($module)) {
    exit 0
}

$sourceFolder = Get-SourceFolderForModule -Module $module
if (-not (Test-Path $sourceFolder)) {
    throw "Source folder not found for module '$module': $sourceFolder"
}

$allowed = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($d in $rulesRaw[$module]) { [void]$allowed.Add([string]$d) }

$violations = New-Object System.Collections.Generic.List[string]
$files = Get-ChildItem -Path $sourceFolder -Recurse -File | Where-Object { $_.Extension -eq '.cpp' -or $_.Extension -eq '.h' }
foreach ($f in $files) {
    $lines = Get-Content $f.FullName
    foreach ($line in $lines) {
        if ($line -match '^\s*#include\s+"([A-Za-z0-9_]+)[/\\]') {
            $depModule = $matches[1]
            if ($depModule -eq $module) { continue }
            if (-not $rulesRaw.ContainsKey($depModule)) { continue }
            if (-not $allowed.Contains($depModule)) {
                $rel = $f.FullName.Substring($SolutionDir.Length).TrimStart('\\')
                $violations.Add("$rel -> $depModule") | Out-Null
            }
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Host "Module dependency violations in ${ProjectName}:"
    $violations | Sort-Object -Unique | ForEach-Object { Write-Host "  $_" }
    throw "Dependency validation failed for module '$module'."
}

exit 0

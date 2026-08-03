param(
    [string]$EnvFile = '.env'
)

$envPath = Join-Path $PSScriptRoot $EnvFile

if (-not (Test-Path $envPath)) {
    throw "Environment file not found: $envPath"
}

Get-Content $envPath | ForEach-Object {
    if ($_ -match '^\s*#' -or [string]::IsNullOrWhiteSpace($_)) {
        return
    }

    $name, $value = $_ -split '=', 2
    $name = $name.Trim()
    $value = $value.Trim().Trim('"').Trim("'")

    if ($name) {
        [System.Environment]::SetEnvironmentVariable($name, $value, 'Process')
    }
}

$requiredVars = @('REMOTE_ADDRESS', 'REMOTE_FOLDER', 'LOCAL_PATH')
foreach ($var in $requiredVars) {
    if ([string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($var, 'Process'))) {
        throw "Missing required setting '$var' in $envPath"
    }
}

$remoteTarget = "$($env:REMOTE_ADDRESS):$($env:REMOTE_FOLDER)"
$localPath = $env:LOCAL_PATH

if (-not (Test-Path $localPath)) {
    throw "Local path does not exist: $localPath"
}

$scpArgs = @()

if ($env:SSH_KEY) {
    $scpArgs += @('-i', $env:SSH_KEY)
}

if ($env:REMOTE_PORT) {
    $scpArgs += @('-P', $env:REMOTE_PORT)
}

if (Test-Path $localPath -PathType Container) {
    $scpArgs += @('-r')
}

$scpArgs += @($localPath, $remoteTarget)

Write-Host "Uploading '$localPath' to '$remoteTarget'"
& scp @scpArgs

if ($LASTEXITCODE -ne 0) {
    throw "scp exited with code $LASTEXITCODE"
}

Write-Host 'Upload completed successfully.'

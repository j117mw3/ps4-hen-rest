$ErrorActionPreference = 'Stop'
$windowsRoot = $PSScriptRoot.Replace('\', '/')
$linuxRoot = (& wsl -d Ubuntu -- wslpath -a $windowsRoot).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Could not resolve the WSL project path.' }
& wsl -d Ubuntu -- make -C $linuxRoot all
if ($LASTEXITCODE -ne 0) { throw 'HEN Rest build failed.' }
$artifacts = @('hen_rest_start.bin', 'hen_rest_hook.prx', 'hen_rest_keeper.prx') | ForEach-Object {
    $artifactPath = Join-Path $PSScriptRoot "dist\$_"
    $file = Get-Item -LiteralPath $artifactPath
    [ordered]@{ name = $file.Name; bytes = $file.Length; sha256 = (Get-FileHash -LiteralPath $artifactPath -Algorithm SHA256).Hash.ToLowerInvariant() }
}
$identityDiagnostic = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'source/start_payload.c') -Raw) -match '(?m)^#define IDENTITY_DIAGNOSTIC 1\s*$'
[ordered]@{ probe_version = '0.3.0'; build_id = '030-launchpad-4'; payload_mode = $(if ($identityDiagnostic) { 'identity-diagnostic-only' } else { 'starter' }); payload_sdk_commit = 'b7326416c23ce14639e9180a24093bdc5eedb579'; built_utc = [DateTime]::UtcNow.ToString('o'); artifacts = @($artifacts) } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'dist\manifest.json') -Encoding utf8
$artifacts | Format-Table

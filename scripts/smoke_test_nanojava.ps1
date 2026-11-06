param(
    [string]$JarDirectory = 'D:\Program\gamejava\j2me',
    [string]$RuntimePath = 'build\nanojava-reference\build\host\nanojava.exe',
    [string]$ClasslibPath = 'build\nanojava-reference\build\classlib.jar',
    [int]$RunMs = 600,
    [string]$OutputDirectory = 'build\j2me-nanojava-smoke'
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$runtime = (Resolve-Path $RuntimePath).Path
$classlib = (Resolve-Path $ClasslibPath).Path
$jarRoot = (Resolve-Path $JarDirectory).Path
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force $outputRoot | Out-Null
New-Item -ItemType Directory -Force (Join-Path $outputRoot 'rms') | Out-Null

# The host runtime was built with MSYS2 MinGW and needs its runtime DLLs.
$env:PATH = 'C:\msys64\mingw32\bin;' + $env:PATH
$env:NANOJAVA_STATS = '1'
$jars = @(Get-ChildItem -LiteralPath $jarRoot -Filter '*.jar' -File | Sort-Object Name)
$rows = [Collections.Generic.List[object]]::new()

function ConvertTo-QuotedArgument([string]$Value) {
    $builder = [Text.StringBuilder]::new()
    [void]$builder.Append('"')
    $slashes = 0
    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq '\') {
            $slashes++
            continue
        }
        if ($character -eq '"') {
            [void]$builder.Append(('\' * (2 * $slashes + 1)))
            [void]$builder.Append('"')
        } else {
            [void]$builder.Append(('\' * $slashes))
            [void]$builder.Append($character)
        }
        $slashes = 0
    }
    [void]$builder.Append(('\' * (2 * $slashes)))
    [void]$builder.Append('"')
    return $builder.ToString()
}

foreach ($jar in $jars) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $runtimeArgs = @('--classlib', $classlib, '--run-ms', $RunMs, '--data', (Join-Path $outputRoot 'rms'), $jar.FullName)
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $runtime
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.Arguments = (($runtimeArgs | ForEach-Object { ConvertTo-QuotedArgument ([string]$_) }) -join ' ')
    $process = [Diagnostics.Process]::Start($startInfo)
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    $exitCode = $process.ExitCode
    $process.Dispose()
    $log = ($stdout + $stderr).Trim()
    $timer.Stop()
    $frames = 0
    $firstFrameMs = $null
    if ($log -match 'nanojava: (\d+) frames') { $frames = [int]$Matches[1] }
    if ($log -match 'first frame (\d+) ms') { $firstFrameMs = [int]$Matches[1] }
    $status = if ($exitCode -eq 0 -and $frames -gt 0) { 'booted+rendered' }
        elseif ($exitCode -eq 0) { 'exited/no-frame' }
        elseif ($log -match 'NoClassDefFoundError') { 'missing-class' }
        elseif ($log -match 'OutOfMemoryError') { 'out-of-memory' }
        elseif ($log -match 'no MIDlet-1 entry') { 'missing-midlet-manifest' }
        else { 'runtime-error' }
    $rows.Add([pscustomobject]@{
        name = $jar.Name
        status = $status
        frames = $frames
        first_frame_ms = $firstFrameMs
        elapsed_ms = $timer.ElapsedMilliseconds
        exit_code = $exitCode
        detail = ($log -replace '[\r\n]+', ' ')
    })
    Write-Progress -Activity 'Smoke-testing J2ME JARs' -Status $jar.Name -PercentComplete (($rows.Count / $jars.Count) * 100)
}

$csvPath = Join-Path $outputRoot 'results.csv'
$jsonPath = Join-Path $outputRoot 'summary.json'
$rows | Export-Csv -NoTypeInformation -Encoding UTF8 $csvPath
$summary = [ordered]@{
    jar_directory = $jarRoot
    jar_count = $jars.Count
    smoke_run_ms = $RunMs
    results = @($rows | Group-Object status | ForEach-Object { [ordered]@{ status = $_.Name; count = $_.Count } })
    first_frame_ms = [ordered]@{
        measured = @($rows | Where-Object { $null -ne $_.first_frame_ms }).Count
        min = ($rows | Where-Object { $null -ne $_.first_frame_ms } | Measure-Object first_frame_ms -Minimum).Minimum
        average = ($rows | Where-Object { $null -ne $_.first_frame_ms } | Measure-Object first_frame_ms -Average).Average
        max = ($rows | Where-Object { $null -ne $_.first_frame_ms } | Measure-Object first_frame_ms -Maximum).Maximum
    }
    csv = $csvPath
}
$summary | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 $jsonPath
$summary | ConvertTo-Json -Depth 5

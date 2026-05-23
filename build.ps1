$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $scriptRoot "build"
$generator = "Visual Studio 17 2022"
$platform = "x64"
$config = "Release"

$cache = Join-Path $buildDir "CMakeCache.txt"
if (Test-Path $cache) {
    $cacheText = Get-Content -Raw $cache
    $expectedGenerator = "CMAKE_GENERATOR:INTERNAL=$generator"
    $expectedPlatform = "CMAKE_GENERATOR_PLATFORM:INTERNAL=$platform"

    if (-not $cacheText.Contains($expectedGenerator) -or -not $cacheText.Contains($expectedPlatform)) {
        Write-Host "Reconfiguring build directory for $generator $platform..."
        @(
            "CMakeCache.txt",
            "CMakeFiles",
            "Makefile",
            "cmake_install.cmake",
            "FocusClock.sln",
            "FocusClock.vcxproj",
            "FocusClock.vcxproj.filters",
            "FocusClock.vcxproj.user",
            "CopyWhitelist.vcxproj",
            "CopyWhitelist.vcxproj.filters",
            "ALL_BUILD.vcxproj",
            "ALL_BUILD.vcxproj.filters",
            "ZERO_CHECK.vcxproj",
            "ZERO_CHECK.vcxproj.filters"
        ) | ForEach-Object {
            $path = Join-Path $buildDir $_
            if (Test-Path $path) {
                Remove-Item -LiteralPath $path -Recurse -Force
            }
        }
    }
}

cmake -S $scriptRoot -B $buildDir -G $generator -A $platform
cmake --build $buildDir --config $config

Write-Host ""
Write-Host "Build complete: $buildDir\FocusClock.exe"

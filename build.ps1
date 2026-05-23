$ErrorActionPreference = "Stop"

$generator = "MinGW Makefiles"
$buildType = "Release"  # 或 "Debug"

cmake -S . -B build -G $generator -DCMAKE_BUILD_TYPE=$buildType
cmake --build build --config $buildType  # --config 对单配置生成器可选但建议保留

Write-Host ""
Write-Host "Build complete: $PWD\build\FocusClock.exe"
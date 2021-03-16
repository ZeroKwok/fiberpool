@echo off
set ProjectName=fiber_pool
set ProjectVersion=0.1.1
set ProjectGenerator32Bit="Visual Studio 16 2019" -A Win32
set ProjectGenerator64Bit="Visual Studio 16 2019" -A x64
set ProjectLibSufix=vc14.2
set ProjectDependPath=G:\local\libraries\boost\boost_1_69_0-msvc-14.1


REM 生成静态运行时32位版本

REM echo on
REM mkdir build_vs2019
REM pushd build_vs2019
REM cmake -G %ProjectGenerator32Bit% ../.. -DCMAKE_INSTALL_PREFIX:PATH="../%ProjectName%_%ProjectVersion%_static_x86_%ProjectLibSufix%" -DFIBERPOOL_ENABLE_STATIC_RUNTIME=ON -DFIBERPOOL_BUILD_EXAMPLE=OFF -DCMAKE_PREFIX_PATH:STRING="%ProjectDependPath%"
REM cmake --build   . --config Debug
REM cmake --build   . --config RelWithDebInfo
REM cmake --install . --config Debug
REM cmake --install . --config RelWithDebInfo
REM popd
REM del /S /Q .\build_vs2019
REM rd /S /Q .\build_vs2019


REM 生成动态运行时32位版本

echo on
mkdir build_vs2019
pushd build_vs2019
cmake -G %ProjectGenerator32Bit% ../.. -DCMAKE_INSTALL_PREFIX:PATH="../%ProjectName%_%ProjectVersion%_dynamic_x86_%ProjectLibSufix%"  -DFIBERPOOL_ENABLE_STATIC_RUNTIME=OFF -DFIBERPOOL_BUILD_EXAMPLE=OFF -DCMAKE_PREFIX_PATH:STRING="%ProjectDependPath%"
cmake --build   . --config Debug
cmake --build   . --config RelWithDebInfo
cmake --install . --config Debug
cmake --install . --config RelWithDebInfo
popd
del /S /Q .\build_vs2019
rd /S /Q .\build_vs2019



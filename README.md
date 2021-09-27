# Ferret Face Game Library
This is a set of utility libraries for writing C++ Windows 10 games. Visual Studio 2022 is required for building these projects.

git clone --recursive https://github.com/spadapet/ff_game_library.git

## Build from Visual Studio 2022
1) Open solution __ff.game.library.sln__
2) Build solution (just Ctrl-Shift-B)
3) Tests show up in the Test Explorer window (Ctrl-E, T)

## Build from the command line
1) Download [nuget.exe](https://dist.nuget.org/win-x86-commandline/latest/nuget.exe)
2) Open a __Visual Studio 2022__ developer command prompt
2) Run: __nuget.exe restore ff.game.library.sln__
3) Run: __msbuild.exe ff.game.library.sln__
    * Or for a specific type of build: __msbuild.exe /p:Configuration=Release|Debug;Platform=x86|x64 ff.game.library.sln__
4) The __out__ directory now contains everything that was built.

## Coding style
* Follow existing code (snake_case, etc)
* Avoid "using namespace" anywhere, use the full::namespace::name or just :: for the global namespace
* Avoid "auto" except for ugly templates where the type is obvious (iterators, shared_ptr, etc)

# Ferret Face Game Library
This is a set of utility libraries for writing C++ Windows 10/11 games. Visual Studio 2022 is required for building these projects.

git clone --recursive https://github.com/spadapet/ff_game_library.git

## Build from Visual Studio 2022
1) Open solution __ff.game.library.sln__
2) Build solution (just Ctrl-Shift-B)
3) Tests show up in the Test Explorer window (Ctrl-E, T)

## Build from the command line
1) Run: __msbuild.exe /r ff.game.library.sln__
    * Add parameter for a specific config: __/p:Configuration=Debug|Profile|Release__
    * __/r__ is only needed the first build to restore packages.
2) The __out__ directory now contains everything that was built.

## Coding style = verbose
* Follow existing code (snake_case, etc)
* Avoid "using namespace" anywhere, use the full::namespace::name or just :: for the global namespace
* Avoid "auto" except for templates where the type is obvious (iterators, shared_ptr, etc)

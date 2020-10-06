# Ferret Face Game Library
This is a set of utility libraries for writing C++ Windows 10 games. Visual Studio 2019 is required for building these projects.

Some libraries may depend on others, like they will all depend on ff.base.
But, for example, games that don't have any audio shouldn't have to link in ff.audio.

## Documentation
* [Doxygen](https://www.doxygen.nl/download.html) is used to build the documentation. A link will be added here if a built copy is hosted on the web.

## Build
1) Download [nuget.exe](https://dist.nuget.org/win-x86-commandline/latest/nuget.exe)
2) Open a __Visual Studio 2019__ developer command prompt
2) Run __nuget.exe restore game_library.sln__
3) Run __msbuild.exe game_library.sln__
    * Or for a specific type of build: __msbuild.exe /p:Configuration=Release|Debug;Platform=x86|x64 game_library.sln__
    * There is also __game_library_uwp.sln__ for Windows 10 UWP builds.
4) The __out__ directory now contains everything that was built.

## Coding style
* Follow the code formatting as defined in .editorconfig
* Casing
    * snake_case for all functions, classes, variables, etc.
    * PascalCase for all template parameter names
* For anything not in the current namespace, use the full::namespace::name or just :: for the global namespace
    * So there shouldn't be any "using namespace" anywhere
* Generally don't use "auto" to be lazy about variable type names except for iterators

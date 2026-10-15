# Ferret Face Game Library
This is a utility library for writing C++ Windows 10/11 games. Visual Studio 2026 is required for building these projects.

git clone --recursive https://github.com/spadapet/ff_game_library.git

## Build from Visual Studio
1) Open solution __ff.game.library.slnx__
2) Build solution (just Ctrl+Shift+B)
3) Tests show up in the Test Explorer window (Ctrl-E, T)

## Build from the command line
1) Run: __msbuild.exe /r ff.game.library.slnx__
    * Add parameter for a specific config: __/p:Configuration=Debug|Profile|Release__
    * __/r__ is only needed the first build to restore packages.
2) The __out__ directory now contains everything that was built.

## ff.base.c arena ownership
Instance-bucket, DX12-heap, CPU/static-buffer, palette-data, and resource-cache initializers take `storage_arena` directly. A non-null arena is borrowed: it must outlive the object and its returned data, and must not be reset or rewound while that data is live. Destruction does not reclaim borrowed allocations. Passing null selects standalone private-arena ownership. GPU-only buffers need no CPU-storage arena. Fence-value initialization also takes a nullable arena, but null selects inline-only storage rather than creating a private arena.

Draw devices share their arena with instance buckets and static-geometry CPU copies. Command-cache recording waits share the resource-tracker arena; submission copies the waits to separate scratch storage before trackers reset. Resource and resource-tracker arenas use the process heap as backing, but retain independent allocation, reset, and destruction lifetimes.

## Coding style = verbose
* Follow existing code (snake_case, etc)
* Avoid "using namespace" anywhere, use the full::namespace::name or just :: for the global namespace
* Avoid "auto" except for templates where the type is obvious (iterators, shared_ptr, etc)

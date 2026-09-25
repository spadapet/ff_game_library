# Copilot instructions for ff_game_library

## General rules

These apply to every project in this repo.

### Comments

- Do not add comments. Write code that explains itself through naming and structure instead.
- The rare exception is a genuinely non-obvious constraint that would otherwise be lost, such as why a specific Win32 flag is required or why an ordering is load-bearing. If you can't point to a concrete surprise the comment is preventing, leave it out.
- Never write comments that restate what the code does, narrate the steps of a function, label sections (e.g. `// Initialization`), or explain a well-known API.
- Do not comment a normalization, workaround, or special case that is fully visible in the code right below it. If a reader can see `if (x == SENTINEL) { x = NULL; }`, a comment saying the sentinel is being normalized adds nothing.
- Do not explain a change in terms of what the code used to do. Comments describe the code as it is now, not its history.
- Don't add file-level or function-level doc-comment banners. Header declarations should stand on their own via clear names and parameter types.

## Project-specific rules

### ff.base.c

`ff.base.c` is a pure C (not C++) reimplementation of the base layer. It's compiled as C, so C++ constructs are unavailable rather than merely discouraged. When working on any file under `source/ff.base.c/`:

- This project doesn't reference other "ff" projects; don't copy patterns from `ff.base` or `ff.base2`. Look at the neighboring files in `ff.base.c` itself for the house style.
- All types are plain old data. Lifetime is managed with explicit `ff_*_init*` / `ff_*_destroy` functions that take a pointer to caller-owned storage (usually a stack local), following `ff_arena`, `ff_dict`, and `ff_string_builder`.
- Prefer a public struct over an opaque `void*` handle. Public fields keep the type debuggable and let callers stack-allocate; treat the fields as read-only from outside the implementation. Don't hide a type behind `void*` just to enforce encapsulation.
- Init functions that can fail return `bool` and must leave the object in a safe, inert state on failure so that calling `destroy` on it is harmless. `destroy` must be idempotent.
- Prefer C types from stdint.h (`uint32_t`, `int64_t`, etc.) and `bool`. Only use Win32 types where an API requires them (`DWORD`, `HANDLE`).
- Use C-style casts.
- Use `static` for internal-linkage globals and functions in .c files. Name file-scope constants with an `s_` prefix (e.g. `s_min_write_capacity`).
- Prefix implementation details that must live in a header with `internal_ff_` (see `arena.h` and `array.h`).
- Allocate from an `ff_arena` rather than calling `malloc` / `HeapAlloc` directly. For a short-lived temporary allocation, initialize an `ff_arena` over a stack buffer with `ff_arena_init_external` so typical cases never touch the heap and only large ones spill.
- `ff_arena_realloc` may relocate a block, so always re-fetch the pointer from its return value.
- Use the `FF_ASSERT_*`, `FF_CHECK_*`, `FF_VERIFY*`, and `FF_DEBUG_FAIL*` macros from `assert.h` for validation and early-out, rather than hand-written `if (...) return;` checks.
- Use designated initializers (`.field = value`) for compound literals; never positional aggregate initializers. Assigning named fields one at a time is also fine.
- Use `ff_string_view` / `ff_wstring_view` for string parameters, never null-terminated `const char*` / `const wchar_t*`. Use `FF_SVL` / `FF_WSVL` for literals and `ff_sz_view` / `ff_wz_view` for runtime C-strings. When an underlying API needs a null-terminated string, make a temporary null-terminated copy internally.
- Use `wchar_t` (not `char16_t`) for wide strings, since every wide Win32/CRT API takes `wchar_t*`.
- Add a `static_assert` on `sizeof` for structs whose layout matters, following `value.c` and `string_builder.c`.
- Add new .c/.h files to both `ff.base.c.vcxproj` and `ff.base.c.vcxproj.filters`, and add public headers to `include/ff.base.c.h`.

#### Headers and includes

- `pch.h` is force-included everywhere and already provides `<stdint.h>`, `<stdbool.h>`, `<stdalign.h>`, `<stdio.h>`, `<stdlib.h>`, `<math.h>`, `<intrin.h>`, plus `<Windows.h>`, `<d3d12.h>`, and `<dxgi1_6.h>`. Don't re-include those in individual files.
- Headers start with `#pragma once` and no include guard.
- Prefer a forward `typedef struct ff_foo ff_foo;` over including another header, the way `arena.h` and `string.h` do for each other's types. Include only when a field's full layout is genuinely needed.
- `COBJMACROS` is defined, so COM interfaces are called through their C macros: `ID3D12Resource_Release(x)`, not `x->Release()` or `x->lpVtbl->Release(x)`.

#### Data structure conventions

- A pointer plus a count travels as `ff_span` (bytes), `ff_array_span`, or `ff_array_slice` from `span.h`. Don't invent a new pair of parameters when one of those fits.
- For a growable array, use the `ff_array_*` macros in `array.h` (`ff_array_init`, `ff_array_push`, `ff_array_count`). The array is arena-allocated and carries its own count and capacity in a header behind the pointer, so a plain `T*` is the array. Because `ff_array_push` can reallocate, never hold a second pointer into an array across a push.
- Return small POD structs by value rather than taking an out-param. C can't return arrays, so a fixed-size array typedef is not an acceptable substitute for a struct; array parameters also silently decay to pointers and lose their size. Use a struct with named fields whenever values are passed or returned, and reserve raw arrays for members *inside* a struct that are indexed rather than named (e.g. a 4x4 matrix as `float m[16]`).
- Give geometry and similar value types an explicit type suffix (`ff_point_float`, `ff_rect_int`) rather than overloading one name, since C has no overloading or templates.

#### Enums, tagged types, and polymorphism

- There is no vtable-based polymorphism. Where the old C++ code used a virtual base, embed a struct tagged with an enum and `switch` on the tag; `dx12_device_child.h` is the reference for this. Keep an `_count` member last in such enums.
- If an enum's order is load-bearing (for example, priority that must match teardown order), that is exactly the kind of non-obvious constraint a comment is allowed to record.

#### Arenas

- `ff_arena_declare_stack(name, size)` declares both the stack buffer and the arena in one step; prefer it to hand-rolling a buffer plus `ff_arena_init_external`.
- Use `ff_arena_alloc_type` / `ff_arena_realloc_type` instead of calling `ff_arena_alloc` with a manual `sizeof` and `alignof`.
- There is no per-allocation free. An arena is freed all at once, so the owner of the arena decides the lifetime of everything in it. Functions that return arena-allocated data must document which arena parameter it came from through the parameter name and ordering, not a comment.

#### Win32 and naming

- File and folder names are lower-case with underscores, grouped by area (`base/`, `data/`, `dx12/`, `windows/`). The `dx12/` files all carry a `dx12_` filename prefix and an `ff_dx12_` symbol prefix.
- Place a type by who can use it, not by what it is: generally useful types go in `base/` (e.g. `ff_point_float`, `ff_rect_float`), while types only the DX12 renderer will ever consume go in `dx12/` (e.g. `ff_color`, `ff_matrix`), even when they look like general-purpose math.
- Prefer the wide (`W`) Win32 entry points directly; there is no `TCHAR` usage.
- Win32 handles that must be released have an explicit `destroy` that tolerates a zeroed struct, so partially-constructed objects can always be torn down on the failure path.

Tests for this project live in `test/ff.test.unit.c/` and are C++ (MSVC CppUnitTest) wrapping the C headers via `extern "C"`. Test files go in `test/ff.test.unit.c/base/`, use `TEST_CLASS` / `TEST_METHOD` inside `namespace ff::test::base`, and must be added to both `ff.test.unit.c.vcxproj` and its `.filters`.

Because the tests compile as C++, any public header has to be valid in both languages. `string.h` shows the pattern: `FF_SVL` has separate `__cplusplus` and C definitions because compound literals and designated initializers aren't spelled the same way in both. Keep new public headers free of C-only syntax in declarations, and if a macro must expand to an initializer, give it both forms.

There is no `.sln`; build individual `.vcxproj` files with MSBuild. Warnings are errors, so a clean build produces no output. A unit test reported as "Skipped" with no result usually means the test host crashed rather than that the test was filtered out.

### ff.base2

The `ff.base2` project has strict constraints. When suggesting or generating code for any file under `source/ff.base2/`:

- The `ff.base2` project doesn't reference other "ff" projects in this solution; don't look at other projects for examples of what to do in `ff.base2`.
- Never use anything from the C++ `std` namespace. Do not suggest `std::string`, `std::vector`, `std::atomic`, `std::unique_ptr`, `<algorithm>`, `<memory>`, `<type_traits>`, etc. Do not include standard library headers that exist solely to provide `std::` types.
- The only C++ language features allowed are namespaces and enum classes (plus the narrow, POD-only function templates noted below for `ff::array`). Everything else should be plain C-style code or Win32 API usage.
- All data types must be plain old data (POD). Structs may contain member functions, but:
  - No constructors (including default, copy, or move constructors).
  - No destructors.
  - No operator overloads that imply non-POD semantics (assignment operators, etc.).
  - Use `init()` and `destroy()` member (or free) functions for lifetime management instead of constructors/destructors.
- Prefer Win32 intrinsics (`InterlockedIncrement`, etc.) over C++ standard equivalents.
- Prefer C types from stdint.h (`uint32_t`, `int64_t`, etc.) and `bool` over Win32 or C++ types. Only use Win32 types when necessary for API compatibility (e.g., `DWORD`, `HANDLE`).
- Use fixed-size C arrays or raw pointers rather than `std::array` / `std::vector`. For dynamic arrays, use the `ff::array` helpers in `array.h` (which are thin, POD-only wrappers over a type-erased core) rather than any C++ container types. Variables that use `ff::array` should have a `_a` suffix to the variable name. Otherwise they just look like pointers.
- Use `_snprintf_s` and similar CRT functions rather than `std::format` or C++ streams.
- Use C-style casts rather than C++ casts (`static_cast`, `reinterpret_cast`, etc.).
- `constexpr` is good for compile-time constants, but avoid `const` variables that require dynamic initialization.
- Avoid any C++ language features that imply non-POD semantics, such as templates, exceptions, RTTI, etc. Stick to plain C-style code with namespaces and enum classes for organization. Exception: the `ff::array` helpers in `array.h` use minimal function templates as thin, POD-only wrappers over a type-erased core (no metaprogramming, no non-POD semantics); hold any new template use to that same bar.
- Always use `this->` in member functions to access member variables, even when not strictly necessary, to maintain clarity and consistency with C-style code.
- Prefer `static` for internal-linkage globals (variables and functions in .cpp files) over anonymous namespaces to maintain a more C-like style and avoid extra indentation.
- Always use `ff::string_view` (or `ff::wstring_view`) for string parameters instead of null-terminated `const char*` / `const wchar_t*`. Never add API overloads that take raw null-terminated string pointers. Callers convert at the call site: use the `FF_SVL` / `FF_WSVL` macros for string literals (compile-time length, no runtime scan) and `ff::sz_view(...)` for runtime null-terminated C-strings. If an underlying API (e.g., a CRT function) genuinely requires a null-terminated string, make a temporary null-terminated copy internally rather than exposing a pointer-based parameter.
- Use `wchar_t` (not `char16_t`) for wide characters and strings. This layer is Windows-only and every wide Win32/CRT API takes `wchar_t*`, so `wchar_t` avoids casts at API boundaries. Use `L"..."` literals (via `FF_WSVL`) and `ff::wstring_view`.
- Do not use positional aggregate initializers that list values in braces or parentheses (e.g. `ff::span{ foo, bar }` or `ff::string_view(a, b)`). Instead, declare the variable and assign each named field explicitly (e.g. `ff::span name; name.data = foo; name.size = bar;`) so the field names are visible. Empty value-initialization like `Type{}` is fine.

When in doubt for `ff.base2`, write the code as if only C with namespaces and enum classes were available.

# Copilot instructions for ff_game_library

## General rules

These apply to every project in this repo.

### Comments

- Do not add comments. Write code that explains itself through naming and structure instead.
- The rare exception is a genuinely non-obvious constraint that would otherwise be lost, such as why a specific Win32 flag is required or why an ordering is load-bearing. If you can't point to a concrete surprise the comment is preventing, leave it out.
- Never write comments that restate what the code does, narrate the steps of a function, label sections (e.g. `// Initialization`), or explain a well-known API.
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

Tests for this project live in `test/ff.test.unit.c/` and are C++ (MSVC CppUnitTest) wrapping the C headers via `extern "C"`. Test files go in `test/ff.test.unit.c/base/`, use `TEST_CLASS` / `TEST_METHOD` inside `namespace ff::test::base`, and must be added to both `ff.test.unit.c.vcxproj` and its `.filters`.

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

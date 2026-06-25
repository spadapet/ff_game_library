# Copilot instructions for ff_game_library

## Project-specific rules

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
- Do not add comments that explain what the code obviously does. Only add comments for non-obvious reasoning, tricky edge cases, or important context. Keep comments minimal.
- Do not use positional aggregate initializers that list values in braces or parentheses (e.g. `ff::span{ foo, bar }` or `ff::string_view(a, b)`). Instead, declare the variable and assign each named field explicitly (e.g. `ff::span name; name.data = foo; name.size = bar;`) so the field names are visible. Empty value-initialization like `Type{}` is fine.

When in doubt for `ff.base2`, write the code as if only C with namespaces and enum classes were available.

#include "pch.h"

// The D3D12 runtime looks for these two exports in the *executable* to decide whether to load
// the Agility SDK redistributable instead of the operating system's D3D12. Without them
// D3D12Core.dll is loaded from system32 and the package has no effect at all.
//
// The version must match the headers being compiled against, which is why it comes from the
// build (D3D12_AGILITY_SDK_VERSION_EXPORT) rather than being written out here. The path is
// relative to the executable, and the NuGet targets copy D3D12Core.dll flat next to it.
//
// This lives in its own file so the whole translation unit is pulled in by the linker: a static
// library only contributes objects that something references, and nothing references these.
// ff_dx12_agility_sdk_version exists purely to give consumers that reference to make.
__declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_AGILITY_SDK_VERSION_EXPORT;
__declspec(dllexport) extern const char* D3D12SDKPath = ".\\";

uint32_t ff_dx12_agility_sdk_version(void)
{
    return (uint32_t)D3D12SDKVersion;
}

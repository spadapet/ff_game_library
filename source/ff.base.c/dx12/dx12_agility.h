#pragma once

// Forces the linker to keep the translation unit that exports D3D12SDKVersion and D3D12SDKPath.
// An executable that wants the Agility SDK runtime has to reference this from code that actually
// runs, otherwise the static library never contributes those exports and D3D12 silently falls
// back to the version in system32.
uint32_t ff_dx12_agility_sdk_version(void);

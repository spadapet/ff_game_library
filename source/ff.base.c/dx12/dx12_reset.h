#pragma once

#include "../base/string.h"

// Recreates the device and rebuilds every live device child against it.
//
// When 'force' is false this only acts if something is actually wrong: a stale DXGI factory, an
// adapter list that changed, or a device that reports removal. A healthy device is left completely
// alone and the call is a no-op, which is what makes it cheap enough to poll between frames.
//
// Returns true when the graphics stack is usable afterward, including the no-op case.
bool ff_dx12_reset_device(bool force);

// Number of completed device resets since ff_dx12_init, for callers that cache anything derived
// from device state.
uint64_t ff_dx12_device_reset_count(void);

// Clears reset state so a later ff_dx12_init starts from a clean slate. Called by ff_dx12_destroy.
void internal_ff_dx12_reset_shutdown(void);

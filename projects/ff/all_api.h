#pragma once

#include <ff/audio/audio_api.h>
#include <ff/base/base_api.h>
#include <ff/data/data_api.h>
#include <ff/graphics/graphics_api.h>
#include <ff/input/input_api.h>
#include <ff/resource/resource_api.h>

#if DXVER == 11 // TODO: include when it supposed DX12
#include <ff/application/application_api.h>
#include <ff/ui/ui_api.h>
#endif

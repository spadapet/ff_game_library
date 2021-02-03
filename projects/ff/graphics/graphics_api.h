#pragma once

#include "pch.h"
#include "source/color.h"
#include "source/data_blob.h"
#include "source/dx_operators.h"
#include "source/dx11_buffer.h"
#include "source/dx11_depth.h"
#include "source/dx11_device_state.h"
#include "source/dx11_fixed_state.h"
#include "source/dx11_texture.h"
#include "source/dx11_texture_view.h"
#include "source/dx11_object_cache.h"
#include "source/dxgi_util.h"
#include "source/graphics.h"
#include "source/graphics_child_base.h"
#include "source/graphics_counters.h"
#include "source/init.h"
#include "source/key_frames.h"
#include "source/matrix.h"
#include "source/matrix_stack.h"
#include "source/palette_data.h"
#include "source/png_image.h"
#include "source/render_target_base.h"
#include "source/render_target_window_base.h"
#include "source/shader.h"
#include "source/sprite_type.h"
#include "source/texture_metadata.h"
#include "source/texture_util.h"
#include "source/transform.h"
#include "source/vertex.h"
#include "source/viewport.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxgi.lib")

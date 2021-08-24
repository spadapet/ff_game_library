#pragma once

#include "pch.h"
#include "source/color.h"
#include "source/data_blob.h"
#include "source/dx_operators.h"
#include "source/dx11_buffer.h"
#include "source/dx11_depth.h"
#include "source/dx11_device_state.h"
#include "source/dx11_fixed_state.h"
#include "source/dx11_object_cache.h"
#include "source/dx11_texture.h"
#include "source/dx12_mem_allocator.h"
#include "source/dx12_mem_range.h"
#include "source/dx12_command_queue.h"
#include "source/dx12_commands.h"
#include "source/dx12_descriptor_allocator.h"
#include "source/dx12_descriptor_range.h"
#include "source/dx12_resource.h"
#include "source/dxgi_util.h"
#include "source/graphics.h"
#include "source/graphics_child_base.h"
#include "source/graphics_counters.h"
#include "source/init.h"
#include "source/matrix.h"
#include "source/matrix_stack.h"
#include "source/png_image.h"
#include "source/shader.h"
#include "source/target_base.h"
#include "source/target_window.h"
#include "source/target_window_base.h"
#include "source/texture_metadata.h"
#include "source/texture_util.h"
#include "source/transform.h"
#include "source/vertex.h"
#include "source/viewport.h"

#if DXVER == 11

#include "source/animation.h"
#include "source/animation_base.h"
#include "source/animation_keys.h"
#include "source/animation_player.h"
#include "source/animation_player_base.h"
#include "source/draw_base.h"
#include "source/draw_device.h"
#include "source/draw_ptr.h"
#include "source/font_file.h"
#include "source/palette_base.h"
#include "source/palette_cycle.h"
#include "source/palette_data.h"
#include "source/random_sprite.h"
#include "source/sprite.h"
#include "source/sprite_base.h"
#include "source/sprite_data.h"
#include "source/sprite_font.h"
#include "source/sprite_list.h"
#include "source/sprite_optimizer.h"
#include "source/sprite_resource.h"
#include "source/sprite_type.h"
#include "source/target_texture.h"
#include "source/texture_view.h"
#include "source/texture_view_base.h"

#endif

#if DXVER == 11
#pragma comment(lib, "d3d11.lib")
#elif DXVER == 12
#pragma comment(lib, "d3d12.lib")
#endif

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxgi.lib")

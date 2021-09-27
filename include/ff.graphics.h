#pragma once

#include "../source/ff.graphics/pch.h"
#include "../source/ff.graphics/source/color.h"
#include "../source/ff.graphics/source/data_blob.h"
#include "../source/ff.graphics/source/dx_operators.h"
#include "../source/ff.graphics/source/dx11_buffer.h"
#include "../source/ff.graphics/source/dx11_depth.h"
#include "../source/ff.graphics/source/dx11_device_state.h"
#include "../source/ff.graphics/source/dx11_fixed_state.h"
#include "../source/ff.graphics/source/dx11_object_cache.h"
#include "../source/ff.graphics/source/dx11_texture.h"
#include "../source/ff.graphics/source/dxgi_util.h"
#include "../source/ff.graphics/source/graphics.h"
#include "../source/ff.graphics/source/graphics_child_base.h"
#include "../source/ff.graphics/source/graphics_counters.h"
#include "../source/ff.graphics/source/init.h"
#include "../source/ff.graphics/source/matrix.h"
#include "../source/ff.graphics/source/matrix_stack.h"
#include "../source/ff.graphics/source/png_image.h"
#include "../source/ff.graphics/source/shader.h"
#include "../source/ff.graphics/source/target_base.h"
#include "../source/ff.graphics/source/target_window.h"
#include "../source/ff.graphics/source/target_window_base.h"
#include "../source/ff.graphics/source/texture_metadata.h"
#include "../source/ff.graphics/source/texture_util.h"
#include "../source/ff.graphics/source/transform.h"
#include "../source/ff.graphics/source/vertex.h"
#include "../source/ff.graphics/source/viewport.h"

#if DXVER == 11

#include "../source/ff.graphics/source/animation.h"
#include "../source/ff.graphics/source/animation_base.h"
#include "../source/ff.graphics/source/animation_keys.h"
#include "../source/ff.graphics/source/animation_player.h"
#include "../source/ff.graphics/source/animation_player_base.h"
#include "../source/ff.graphics/source/draw_base.h"
#include "../source/ff.graphics/source/draw_device.h"
#include "../source/ff.graphics/source/draw_ptr.h"
#include "../source/ff.graphics/source/font_file.h"
#include "../source/ff.graphics/source/palette_base.h"
#include "../source/ff.graphics/source/palette_cycle.h"
#include "../source/ff.graphics/source/palette_data.h"
#include "../source/ff.graphics/source/random_sprite.h"
#include "../source/ff.graphics/source/sprite.h"
#include "../source/ff.graphics/source/sprite_base.h"
#include "../source/ff.graphics/source/sprite_data.h"
#include "../source/ff.graphics/source/sprite_font.h"
#include "../source/ff.graphics/source/sprite_list.h"
#include "../source/ff.graphics/source/sprite_optimizer.h"
#include "../source/ff.graphics/source/sprite_resource.h"
#include "../source/ff.graphics/source/sprite_type.h"
#include "../source/ff.graphics/source/target_texture.h"
#include "../source/ff.graphics/source/texture_view.h"
#include "../source/ff.graphics/source/texture_view_base.h"

#endif

#if DXVER == 11
#pragma comment(lib, "d3d11.lib")
#elif DXVER == 12
#pragma comment(lib, "d3d12.lib")
#endif

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxgi.lib")

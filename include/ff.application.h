#pragma once

#include "../source/ff.application/pch.h"
#include "../source/ff.application/init_app.h"
#include "../source/ff.application/init_dx.h"

#include "../source/ff.application/app/app.h"
#include "../source/ff.application/app/debug_stats.h"
#include "../source/ff.application/app/game.h"
#include "../source/ff.application/app/imgui.h"
#include "../source/ff.application/app/settings.h"

#include "../source/ff.application/audio/audio.h"
#include "../source/ff.application/audio/audio_child_base.h"
#include "../source/ff.application/audio/audio_effect.h"
#include "../source/ff.application/audio/audio_effect_base.h"
#include "../source/ff.application/audio/audio_effect_playing.h"
#include "../source/ff.application/audio/audio_playing_base.h"
#include "../source/ff.application/audio/destroy_voice.h"
#include "../source/ff.application/audio/music.h"
#include "../source/ff.application/audio/music_playing.h"
#include "../source/ff.application/audio/wav_file.h"

#include "../source/ff.application/dx_types/blob.h"
#include "../source/ff.application/dx_types/color.h"
#include "../source/ff.application/dx_types/intrusive_list.h"
#include "../source/ff.application/dx_types/matrix.h"
#include "../source/ff.application/dx_types/operators.h"
#include "../source/ff.application/dx_types/transform.h"
#include "../source/ff.application/dx_types/viewport.h"

#include "../source/ff.application/dx12/access.h"
#include "../source/ff.application/dx12/buffer.h"
#include "../source/ff.application/dx12/commands.h"
#include "../source/ff.application/dx12/depth.h"
#include "../source/ff.application/dx12/descriptor_allocator.h"
#include "../source/ff.application/dx12/descriptor_range.h"
#include "../source/ff.application/dx12/device_reset_priority.h"
#include "../source/ff.application/dx12/draw_device.h"
#include "../source/ff.application/dx12/dx12_globals.h"
#include "../source/ff.application/dx12/fence.h"
#include "../source/ff.application/dx12/fence_value.h"
#include "../source/ff.application/dx12/fence_values.h"
#include "../source/ff.application/dx12/gpu_event.h"
#include "../source/ff.application/dx12/heap.h"
#include "../source/ff.application/dx12/mem_allocator.h"
#include "../source/ff.application/dx12/mem_range.h"
#include "../source/ff.application/dx12/object_cache.h"
#include "../source/ff.application/dx12/queue.h"
#include "../source/ff.application/dx12/queues.h"
#include "../source/ff.application/dx12/residency.h"
#include "../source/ff.application/dx12/resource.h"
#include "../source/ff.application/dx12/resource_state.h"
#include "../source/ff.application/dx12/resource_tracker.h"
#include "../source/ff.application/dx12/target_texture.h"
#include "../source/ff.application/dx12/target_window.h"
#include "../source/ff.application/dx12/texture.h"
#include "../source/ff.application/dx12/texture_view.h"

#include "../source/ff.application/dxgi/buffer_base.h"
#include "../source/ff.application/dxgi/command_context_base.h"
#include "../source/ff.application/dxgi/depth_base.h"
#include "../source/ff.application/dxgi/device_child_base.h"
#include "../source/ff.application/dxgi/draw_base.h"
#include "../source/ff.application/dxgi/draw_device_base.h"
#include "../source/ff.application/dxgi/draw_util.h"
#include "../source/ff.application/dxgi/dxgi_globals.h"
#include "../source/ff.application/dxgi/format_util.h"
#include "../source/ff.application/dxgi/palette_base.h"
#include "../source/ff.application/dxgi/palette_data_base.h"
#include "../source/ff.application/dxgi/sprite_data.h"
#include "../source/ff.application/dxgi/target_access_base.h"
#include "../source/ff.application/dxgi/target_base.h"
#include "../source/ff.application/dxgi/target_window_base.h"
#include "../source/ff.application/dxgi/texture_base.h"
#include "../source/ff.application/dxgi/texture_metadata_base.h"
#include "../source/ff.application/dxgi/texture_view_access_base.h"
#include "../source/ff.application/dxgi/texture_view_base.h"

#include "../source/ff.application/graphics/animation.h"
#include "../source/ff.application/graphics/animation_base.h"
#include "../source/ff.application/graphics/animation_keys.h"
#include "../source/ff.application/graphics/animation_player.h"
#include "../source/ff.application/graphics/animation_player_base.h"
#include "../source/ff.application/graphics/palette_cycle.h"
#include "../source/ff.application/graphics/palette_data.h"
#include "../source/ff.application/graphics/png_image.h"
#include "../source/ff.application/graphics/random_sprite.h"
#include "../source/ff.application/graphics/shader.h"
#include "../source/ff.application/graphics/sprite.h"
#include "../source/ff.application/graphics/sprite_base.h"
#include "../source/ff.application/graphics/sprite_font.h"
#include "../source/ff.application/graphics/sprite_list.h"
#include "../source/ff.application/graphics/sprite_optimizer.h"
#include "../source/ff.application/graphics/sprite_resource.h"
#include "../source/ff.application/graphics/texture_data.h"
#include "../source/ff.application/graphics/texture_metadata.h"
#include "../source/ff.application/graphics/texture_resource.h"

#include "../source/ff.application/input/gamepad_device.h"
#include "../source/ff.application/input/input.h"
#include "../source/ff.application/input/input_device_base.h"
#include "../source/ff.application/input/input_device_event.h"
#include "../source/ff.application/input/input_mapping.h"
#include "../source/ff.application/input/input_vk.h"
#include "../source/ff.application/input/keyboard_device.h"
#include "../source/ff.application/input/pointer_device.h"

#include "../source/ff.application/write/font_file.h"
#include "../source/ff.application/write/write.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "xinput.lib")

#if PROFILE_APP
#pragma comment(lib, "WinPixEventRuntime.lib")
#endif

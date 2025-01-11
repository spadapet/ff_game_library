#pragma once

namespace ff::internal
{
    ff::co_task<> destroy_voice_async(IXAudio2SourceVoice* source);
}

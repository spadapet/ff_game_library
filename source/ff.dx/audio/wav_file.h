#pragma once

namespace ff::internal
{
    std::shared_ptr<ff::saved_data_base> read_wav_file(ff::reader_base& reader, WAVEFORMATEX& format);
}

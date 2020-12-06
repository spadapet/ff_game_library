#pragma once

namespace ff
{
    class data_base;
    class reader_base;
    class writer_base;
}

namespace ff::compression
{
    bool compress(reader_base& reader, size_t full_size, writer_base& writer);
    bool uncompress(reader_base& reader, size_t saved_size, writer_base& writer);

    std::shared_ptr<data_base> decode_base64(std::string_view text);
}

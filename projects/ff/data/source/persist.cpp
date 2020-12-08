#include "pch.h"
#include "data.h"
#include "persist.h"
#include "saved_data.h"
#include "stream.h"

bool ff::load_bytes(reader_base& reader, void* data, size_t size)
{
    return reader.read(data, size) && ff::load_padding(reader, size);
}

bool ff::load_bytes(reader_base& reader, size_t size, std::shared_ptr<data_base>& data)
{
    auto saved_data = reader.saved_data(reader.pos(), size, size, saved_data_type::none);
    if (saved_data)
    {
        data = saved_data->saved_data();
        if (data)
        {
            size_t new_pos = reader.pos() + size;
            return reader.pos(reader.pos() + size) == new_pos;
        }
    }

    return false;
}

bool ff::load_padding(reader_base& reader, size_t size_read)
{
    static std::array<uint8_t, 4> padding;
    size_t padding_size = ff::math::round_up<size_t>(size_read, 4) - size_read;
    return reader.read(padding.data(), padding_size) == padding_size;
}

bool ff::save_bytes(writer_base& writer, const void* data, size_t size)
{
    return writer.write(data, size) == size && ff::save_padding(writer, size);
}

bool ff::save_bytes(writer_base& writer, const data_base& data)
{
    return ff::save_bytes(writer, data.data(), data.size());
}

bool ff::save_padding(writer_base& writer, size_t size_written)
{
    static const std::array<uint8_t, 4> padding = { 0, 0, 0, 0 };
    size_t padding_size = ff::math::round_up<size_t>(size_written, 4) - size_written;
    return writer.write(padding.data(), padding_size) == padding_size;
}

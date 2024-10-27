#include "pch.h"
#include "base/assert.h"
#include "data_persist/compression.h"
#include "data_persist/data.h"
#include "data_persist/stream.h"
#include <zlib/zlib.h>

static size_t get_chunk_size_for_data_size(size_t data_size)
{
    static const size_t max_chunk_size = 1024 * 256;
    return std::min<size_t>(data_size, max_chunk_size);
}

bool ff::compression::compress(reader_base& reader, size_t full_size, writer_base& writer)
{
    z_stream zlib_data{};
    deflateInit(&zlib_data, Z_BEST_COMPRESSION);

    const size_t chunk_size = ::get_chunk_size_for_data_size(full_size);
    std::vector<uint8_t> output_chunk(chunk_size);
    std::vector<uint8_t> input_chunk(chunk_size);
    bool status = true;

    for (size_t pos = 0; status && pos < full_size; pos += chunk_size)
    {
        size_t read_size = std::min(full_size - pos, chunk_size);
        size_t actually_read_size = reader.read(input_chunk.data(), read_size);
        bool last_chunk = pos + chunk_size >= full_size;

        if (read_size == actually_read_size)
        {
            zlib_data.avail_in = static_cast<uInt>(read_size);
            zlib_data.next_in = input_chunk.data();

            do
            {
                zlib_data.avail_out = static_cast<uInt>(output_chunk.size());
                zlib_data.next_out = output_chunk.data();
                deflate(&zlib_data, last_chunk ? Z_FINISH : Z_NO_FLUSH);

                size_t write_size = output_chunk.size() - zlib_data.avail_out;
                if (write_size)
                {
                    status = (writer.write(output_chunk.data(), write_size) == write_size);
                }
            }
            while (status && !zlib_data.avail_out);
            assert(!zlib_data.avail_in);
        }
        else
        {
            assert(false);
            status = false;
        }
    }

    deflateEnd(&zlib_data);
    return status;
}

bool ff::compression::uncompress(reader_base& reader, size_t saved_size, writer_base& writer)
{
    if (!saved_size)
    {
        return true;
    }

    z_stream zlib_data{};
    inflateInit(&zlib_data);

    const size_t chunk_size = ::get_chunk_size_for_data_size(saved_size);
    std::vector<uint8_t> input_chunk(chunk_size);
    std::vector<uint8_t> output_chunk(chunk_size * 2);
    bool status = true;
    int inflate_status = Z_OK;
    size_t pos = 0;

    for (; status && pos < saved_size; pos = std::min(pos + chunk_size, saved_size))
    {
        // Read a chunk of input and get ready to pass it to zlib
        size_t read_size = std::min(saved_size - pos, chunk_size);
        size_t actually_read_size = reader.read(input_chunk.data(), read_size);

        if (read_size == actually_read_size)
        {
            zlib_data.avail_in = static_cast<uInt>(read_size);
            zlib_data.next_in = input_chunk.data();

            do
            {
                zlib_data.avail_out = static_cast<uInt>(output_chunk.size());
                zlib_data.next_out = output_chunk.data();
                inflate_status = inflate(&zlib_data, Z_NO_FLUSH);

                status = inflate_status != Z_NEED_DICT && inflate_status != Z_DATA_ERROR && inflate_status != Z_MEM_ERROR;
                if (status)
                {
                    size_t write_size = output_chunk.size() - zlib_data.avail_out;
                    if (write_size)
                    {
                        status = (writer.write(output_chunk.data(), write_size) == write_size);
                    }
                }
            }
            while (status && !zlib_data.avail_out);
            assert(!zlib_data.avail_in);
        }
        else
        {
            assert(false);
            status = false;
        }
    }

    status = (pos == saved_size && inflate_status == Z_STREAM_END);
    assert(status);

    inflateEnd(&zlib_data);
    return status;
}

static uint8_t CHAR_TO_BYTE[] =
{
    62, // +
    0,
    0,
    0,
    63, // /
    52, // 0
    53, // 1
    54, // 2
    55, // 3
    56, // 4
    57, // 5
    58, // 6
    59, // 7
    60, // 8
    61, // 9
    0, // :
    0, // ;
    0, // <
    0, // =
    0, // >
    0, // ?
    0, // @
    0, // A
    1, // B
    2, // C
    3, // D
    4, // E
    5, // F
    6, // G
    7, // H
    8, // I
    9, // J
    10, // K
    11, // L
    12, // M
    13, // N
    14, // O
    15, // P
    16, // Q
    17, // R
    18, // S
    19, // T
    20, // U
    21, // V
    22, // W
    23, // X
    24, // Y
    25, // Z
    0, // [
    0, // slash
    0, // ]
    0, // ^
    0, // _
    0, // `
    26, // a
    27, // b
    28, // c
    29, // d
    30, // e
    31, // f
    32, // g
    33, // h
    34, // i
    35, // j
    36, // k
    37, // l
    38, // m
    39, // n
    40, // o
    41, // p
    42, // q
    43, // r
    44, // s
    45, // t
    46, // u
    47, // v
    48, // w
    49, // x
    50, // y
    51, // z
};

inline static uint8_t char_to_byte(char ch)
{
    if (ch >= '+' && ch <= 'z')
    {
        return ::CHAR_TO_BYTE[ch - '+'];
    }

    return 0;
}

std::shared_ptr<ff::data_base> ff::compression::decode_base64(std::string_view text)
{
    // Input string needs to be properly padded
    if (text.size() % 4 != 0)
    {
        assert(false);
        return nullptr;
    }

    std::vector<uint8_t> out;
    out.resize(text.size() / 4 * 3);
    uint8_t* pout = out.data();

    const char* end = text.data() + text.size();
    for (const char* ch = text.data(); ch < end; ch += 4, pout += 3)
    {
        uint8_t ch0 = ::char_to_byte(ch[0]);
        uint8_t ch1 = ::char_to_byte(ch[1]);
        uint8_t ch2 = ::char_to_byte(ch[2]);
        uint8_t ch3 = ::char_to_byte(ch[3]);

        pout[0] = ((ch0 & 0b00111111) << 2) | ((ch1 & 0b00110000) >> 4);
        pout[1] = ((ch1 & 0b00001111) << 4) | ((ch2 & 0b00111100) >> 2);
        pout[2] = ((ch2 & 0b00000011) << 6) | ((ch3 & 0b00111111) >> 0);
    }

    if (text.size())
    {
        if (end[-2] == '=')
        {
            assert(!out.back());
            out.pop_back();
        }

        if (end[-1] == '=')
        {
            assert(!out.back());
            out.pop_back();
        }
    }

    return std::make_shared<ff::data_vector>(std::move(out));
}

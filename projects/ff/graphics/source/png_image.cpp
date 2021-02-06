#include "pch.h"
#include "png_image.h"

ff::png_image_reader::png_image_reader(const uint8_t* bytes, size_t size)
    : png(nullptr)
    , info(nullptr)
    , end_info(nullptr)
    , read_pos(bytes)
    , end_pos(bytes + size)
    , width(0)
    , height(0)
    , bit_depth(0)
    , color_type(0)
    , interlate_method(0)
    , has_palette(false)
    , palette_(nullptr)
    , palette_size(0)
    , has_trans_palette(false)
    , trans_palette(nullptr)
    , trans_palette_size(0)
    , trans_color(nullptr)
{
    this->png = ::png_create_read_struct(PNG_LIBPNG_VER_STRING, this, &png_image_reader::png_error_callback, &png_image_reader::png_warning_callback);
    this->info = ::png_create_info_struct(this->png);
    this->end_info = ::png_create_info_struct(this->png);
}

ff::png_image_reader::~png_image_reader()
{
    ::png_destroy_read_struct(&this->png, &this->info, &this->end_info);
}

std::unique_ptr<DirectX::ScratchImage> ff::png_image_reader::read(DXGI_FORMAT requested_format)
{
    std::unique_ptr<DirectX::ScratchImage> scratch;

    try
    {
        scratch = this->internal_read(requested_format);
        if (!scratch && this->error_.empty())
        {
            this->error_ = "Failed to read PNG data";
        }
    }
    catch (std::string errorText)
    {
        this->error_ = !errorText.empty() ? errorText : std::string("Failed to read PNG data");
    }

    return scratch;
}

std::unique_ptr<DirectX::ScratchImage> ff::png_image_reader::pallete() const
{
    std::unique_ptr<DirectX::ScratchImage> scratch;

    if (this->has_palette)
    {
        scratch = std::make_unique<DirectX::ScratchImage>();
        if (FAILED(scratch->Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 256, 1, 1, 1)))
        {
            assert(false);
            return nullptr;
        }

        const DirectX::Image& image = *scratch->GetImages();
        std::memset(image.pixels, 0, image.rowPitch * image.height);

        for (size_t i = 0; i < (size_t)this->palette_size; i++)
        {
            image.pixels[i * 4 + 0] = this->palette_[i].red;
            image.pixels[i * 4 + 1] = this->palette_[i].green;
            image.pixels[i * 4 + 2] = this->palette_[i].blue;
            image.pixels[i * 4 + 3] = (this->has_trans_palette && i < (size_t)this->trans_palette_size) ? this->trans_palette[i] : 0xFF;
        }
    }

    return scratch;
}

const std::string& ff::png_image_reader::error() const
{
    return this->error_;
}

std::unique_ptr<DirectX::ScratchImage> ff::png_image_reader::internal_read(DXGI_FORMAT requested_format)
{
    if (::png_sig_cmp(this->read_pos, 0, this->end_pos - this->read_pos) != 0)
    {
        return nullptr;
    }

    ::png_set_read_fn(this->png, this, &png_image_reader::png_read_callback);
    ::png_set_keep_unknown_chunks(this->png, PNG_HANDLE_CHUNK_NEVER, nullptr, 0);
    ::png_read_info(this->png, this->info);

    // General image info
    if (!::png_get_IHDR(
        this->png,
        this->info,
        &this->width,
        &this->height,
        &this->bit_depth,
        &this->color_type,
        &this->interlate_method,
        nullptr,
        nullptr))
    {
        return nullptr;
    }

    // Palette
    this->has_palette = ::png_get_PLTE(this->png, this->info, &this->palette_, &this->palette_size) != 0;
    this->has_trans_palette = ::png_get_tRNS(this->png, this->info, &this->trans_palette, &this->trans_palette_size, &this->trans_color) != 0;

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    switch (this->color_type)
    {
        default:
            this->error_ = "Invalid color type";
            return nullptr;

        case PNG_COLOR_TYPE_GRAY:
            format = (this->bit_depth == 1) ? DXGI_FORMAT_R1_UNORM : DXGI_FORMAT_R8_UNORM;
            if (requested_format != format)
            {
                ::png_set_gray_to_rgb(this->png);
                ::png_set_add_alpha(this->png, 0xFF, PNG_FILLER_AFTER);
                format = DXGI_FORMAT_R8G8B8A8_UNORM;
            }
            else if (this->bit_depth > 1 && this->bit_depth < 8)
            {
                ::png_set_expand_gray_1_2_4_to_8(this->png);
            }
            break;

        case PNG_COLOR_TYPE_PALETTE:
            format = DXGI_FORMAT_R8_UINT;
            if (requested_format != format)
            {
                ::png_set_palette_to_rgb(this->png);

                if (this->has_trans_palette)
                {
                    ::png_set_tRNS_to_alpha(this->png);
                }
                else
                {
                    ::png_set_add_alpha(this->png, 0xFF, PNG_FILLER_AFTER);
                }

                format = DXGI_FORMAT_R8G8B8A8_UNORM;
            }
            else if (this->palette_size < 256)
            {
                ::png_set_packing(this->png);
            }
            break;

        case PNG_COLOR_TYPE_RGB:
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
            ::png_set_add_alpha(this->png, 0xFF, PNG_FILLER_AFTER);
            break;

        case PNG_COLOR_TYPE_RGB_ALPHA:
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;

        case PNG_COLOR_TYPE_GRAY_ALPHA:
            ::png_set_gray_to_rgb(this->png);
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
    }

    std::unique_ptr<DirectX::ScratchImage> scratch = std::make_unique<DirectX::ScratchImage>();
    if (FAILED(scratch->Initialize2D(format, this->width, this->height, 1, 1)))
    {
        return nullptr;
    }

    this->rows.resize(this->height);
    {
        const DirectX::Image& image = *scratch->GetImage(0, 0, 0);
        BYTE* imagePixels = image.pixels;

        for (unsigned int i = 0; i < this->height; i++)
        {
            this->rows[i] = &imagePixels[i * image.rowPitch];
        }
    }

    ::png_read_image(this->png, this->rows.data());
    ::png_read_end(this->png, this->end_info);

    return scratch;
}

void ff::png_image_reader::png_error_callback(png_struct* png, const char* text)
{
    png_image_reader* info = reinterpret_cast<png_image_reader*>(::png_get_error_ptr(png));
    info->on_png_error(text);
}

void ff::png_image_reader::png_warning_callback(png_struct* png, const char* text)
{
    png_image_reader* info = reinterpret_cast<png_image_reader*>(::png_get_error_ptr(png));
    info->on_png_warning(text);
}

void ff::png_image_reader::png_read_callback(png_struct* png, uint8_t* data, size_t size)
{
    png_image_reader* info = reinterpret_cast<png_image_reader*>(::png_get_io_ptr(png));
    info->on_png_read(data, size);
}

void ff::png_image_reader::on_png_error(const char* text)
{
    assert(false);
    throw std::string(text);
}

void ff::png_image_reader::on_png_warning(const char* text)
{
    assert(false);
}

void ff::png_image_reader::on_png_read(uint8_t* data, size_t size)
{
    size = std::min<size_t>(size, this->end_pos - this->read_pos);
    ::memcpy(data, this->read_pos, size);
    this->read_pos += size;
}

ff::png_image_writer::png_image_writer(ff::writer_base& writer)
    : writer(writer)
{
    this->png = ::png_create_write_struct(PNG_LIBPNG_VER_STRING, this, &png_image_writer::png_error_callback, &png_image_writer::png_warning_callback);
    this->info = ::png_create_info_struct(this->png);
}

ff::png_image_writer::~png_image_writer()
{
    ::png_destroy_write_struct(&this->png, &this->info);
}

bool ff::png_image_writer::write(const DirectX::Image& image, const DirectX::Image* palette_image)
{
    try
    {
        return internal_write(image, palette_image);
    }
    catch (std::string errorText)
    {
        this->error_ = !errorText.empty() ? errorText : std::string("Failed to write PNG data");
        return false;
    }
}

const std::string& ff::png_image_writer::error() const
{
    return this->error_;
}

bool ff::png_image_writer::internal_write(const DirectX::Image& image, const DirectX::Image* palette_image)
{
    int bitDepth = 8;
    int colorType;

    switch (image.format)
    {
        case DXGI_FORMAT_R8_UINT:
            colorType = PNG_COLOR_TYPE_PALETTE;
            break;

        case DXGI_FORMAT_R8G8B8A8_UNORM:
            colorType = PNG_COLOR_TYPE_RGB_ALPHA;
            break;

        default:
            this->error_ = "Unsupported texture format for saving to PNG";
            break;
    }

    ::png_set_write_fn(this->png, this, &png_image_writer::png_write_callback, &png_image_writer::png_flush_callback);

    ::png_set_IHDR(
        this->png,
        this->info,
        (unsigned int)image.width,
        (unsigned int)image.height,
        bitDepth,
        colorType,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);

    if (palette_image)
    {
        size_t colorCount = palette_image->width;
        ::png_color_16 transColor{ 0 };
        bool foundTrans = false;

        std::vector<::png_color> colors;
        colors.resize(colorCount);

        std::vector<BYTE> trans;
        trans.resize(colorCount);

        const BYTE* src = palette_image->pixels;
        for (size_t i = 0; i < colorCount; i++, src += 4)
        {
            colors[i].red = src[0];
            colors[i].green = src[1];
            colors[i].blue = src[2];

            if (colorType == PNG_COLOR_TYPE_PALETTE)
            {
                trans[i] = src[3];
            }
        }

        ::png_set_PLTE(this->png, this->info, colors.data(), static_cast<int>(colors.size()));

        if (foundTrans && colorType == PNG_COLOR_TYPE_PALETTE)
        {
            ::png_set_tRNS(this->png, this->info, trans.data(), static_cast<int>(trans.size()), &transColor);
        }
    }

    this->rows.resize(image.height);

    for (size_t i = 0; i < image.height; i++)
    {
        this->rows[i] = &image.pixels[i * image.rowPitch];
    }

    ::png_set_rows(this->png, this->info, this->rows.data());
    ::png_write_png(this->png, this->info, PNG_TRANSFORM_IDENTITY, nullptr);

    return true;
}

void ff::png_image_writer::png_error_callback(png_struct* png, const char* text)
{
    png_image_writer* info = reinterpret_cast<png_image_writer*>(::png_get_error_ptr(png));
    info->on_png_error(text);
}

void ff::png_image_writer::png_warning_callback(png_struct* png, const char* text)
{
    png_image_writer* info = reinterpret_cast<png_image_writer*>(::png_get_error_ptr(png));
    info->on_png_warning(text);
}

void ff::png_image_writer::png_write_callback(png_struct* png, uint8_t* data, size_t size)
{
    png_image_writer* info = reinterpret_cast<png_image_writer*>(::png_get_io_ptr(png));
    info->on_png_write(data, size);
}

void ff::png_image_writer::png_flush_callback(png_struct* png)
{
    // not needed
}

void ff::png_image_writer::on_png_error(const char* text)
{
    assert(false);
    throw std::string(text);
}

void ff::png_image_writer::on_png_warning(const char* text)
{
    assert(false);
}

void ff::png_image_writer::on_png_write(const uint8_t* data, size_t size)
{
    this->writer.write(data, size);
}

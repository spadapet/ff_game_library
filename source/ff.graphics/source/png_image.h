#pragma once

namespace ff
{
    class png_image_reader
    {
    public:
        png_image_reader(const uint8_t* bytes, size_t size);
        ~png_image_reader();

        std::unique_ptr<DirectX::ScratchImage> read(DXGI_FORMAT requested_format = DXGI_FORMAT_UNKNOWN);
        std::unique_ptr<DirectX::ScratchImage> pallete() const;
        const std::string& error() const;

    private:
        std::unique_ptr<DirectX::ScratchImage> internal_read(DXGI_FORMAT requested_format);

        static void png_error_callback(png_struct* png, const char* text);
        static void png_warning_callback(png_struct* png, const char* text);
        static void png_read_callback(png_struct* png, uint8_t* data, size_t size);

        void on_png_error(const char* text);
        void on_png_warning(const char* text);
        void on_png_read(uint8_t* data, size_t size);

        // Data
        png_struct* png;
        png_info* info;
        png_info* end_info;
        std::string error_;

        // Reading
        const uint8_t* read_pos;
        const uint8_t* end_pos;
        std::vector<BYTE*> rows;

        // Properties
        unsigned int width;
        unsigned int height;
        int bit_depth;
        int color_type;
        int interlate_method;

        // Palette
        bool has_palette;
        png_color* palette_;
        int palette_size;
        bool has_trans_palette;
        uint8_t* trans_palette;
        int trans_palette_size;
        png_color_16* trans_color;
    };

    class png_image_writer
    {
    public:
        png_image_writer(ff::writer_base& writer);
        ~png_image_writer();

        bool write(const DirectX::Image& image, const DirectX::Image* palette_image);
        const std::string& error() const;

    private:
        bool internal_write(const DirectX::Image& image, const DirectX::Image* palette_image);

        static void png_error_callback(png_struct* png, const char* text);
        static void png_warning_callback(png_struct* png, const char* text);
        static void png_write_callback(png_struct* png, uint8_t* data, size_t size);
        static void png_flush_callback(png_struct* png);

        void on_png_error(const char* text);
        void on_png_warning(const char* text);
        void on_png_write(const uint8_t* data, size_t size);

        // Data
        png_struct* png;
        png_info* info;
        std::string error_;

        // Writing
        ff::writer_base& writer;
        std::vector<BYTE*> rows;
    };
}

#pragma once

namespace ff
{
    IDWriteFactory7* write_factory();
    IDWriteInMemoryFontFileLoader* write_font_loader();
}

namespace ff::internal::write
{
    bool init();
    void destroy();
}

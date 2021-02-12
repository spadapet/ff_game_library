#include "pch.h"
#include "source/utility.h"

namespace graphics_test
{
    TEST_CLASS(texture_test)
    {
    public:
        TEST_METHOD(texture_resource)
        {
            std::string_view json_source =
                "{\n"
                "    'test_texture': { 'res:type': 'texture', 'file': 'file:test_texture.png', 'format': 'bc3', 'mips': '4' }\n"
                "}\n";
            auto result = ff::test::create_resources(json_source);
            auto& res = std::get<0>(result);

            ff::auto_resource<ff::dx11_texture> texture_res = res->get_resource_object("test_texture");
            Assert::IsTrue(texture_res.valid());

            std::shared_ptr<ff::dx11_texture> texture = texture_res.object();
            Assert::IsNotNull(texture.get());
            Assert::IsTrue(texture->format() == DXGI_FORMAT_BC3_UNORM);
            Assert::IsTrue(texture->size() == ff::point_int(256, 256));
            Assert::IsTrue(texture->array_size() == 1);
            Assert::IsTrue(texture->sample_count() == 1);
            Assert::IsTrue(texture->mip_count() == 4);
            Assert::IsNotNull(texture->texture());
            Assert::IsNotNull(texture->view());
            Assert::IsNotNull(texture->animation());
        }

        TEST_METHOD(convert)
        {
            ff::resource_file file(".png", ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_TEXTURE));
            ff::dx11_texture texture(file);
            Assert::IsTrue(texture.format() == ff::internal::DEFAULT_FORMAT);

            ff::dx11_texture converted_texture(texture, DXGI_FORMAT_BC1_UNORM, 2);
            Assert::IsTrue(converted_texture.format() == DXGI_FORMAT_BC1_UNORM);
            Assert::IsTrue(converted_texture.size() == texture.size());
            Assert::IsTrue(converted_texture.mip_count() == 2);
        }
    };
}

#pragma once

namespace ff::test
{
    void remove_temp_path();
    std::tuple<std::shared_ptr<ff::resource_objects>, std::filesystem::path, ff::scope_exit> create_resources(std::string_view json_source);
    void assert_image(const std::filesystem::path& actual_path, DWORD expect_resource_id);
}

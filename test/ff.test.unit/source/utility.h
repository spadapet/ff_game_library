#pragma once

namespace ff::test
{
    void remove_temp_path();
    std::tuple<std::unique_ptr<ff::resource_objects>, std::filesystem::path, ff::scope_exit> create_resources(std::string_view json_source);
}

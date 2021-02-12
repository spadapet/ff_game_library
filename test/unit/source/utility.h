#pragma once

namespace ff::test
{
    std::tuple<std::unique_ptr<ff::resource_objects>, ff::end_scope_action> create_resources(std::string_view json_source);
}

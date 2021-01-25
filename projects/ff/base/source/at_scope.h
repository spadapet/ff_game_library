#pragma once

namespace ff
{
    class at_scope
    {
    public:
        at_scope(std::function<void()>&& func);
        at_scope(at_scope&& other) noexcept = default;
        at_scope(const at_scope& other) = delete;
        ~at_scope();

        at_scope& operator=(at_scope&& other) noexcept = default;
        at_scope& operator=(const at_scope& other) = delete;

    private:
        std::function<void()> func;
    };
}

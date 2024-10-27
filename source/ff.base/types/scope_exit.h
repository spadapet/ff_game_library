#pragma once

namespace ff
{
    class scope_exit
    {
    public:
        scope_exit(std::function<void()>&& func);
        scope_exit(scope_exit&& other) noexcept = default;
        scope_exit(const scope_exit& other) = delete;
        ~scope_exit();

        scope_exit& operator=(scope_exit&& other) noexcept = default;
        scope_exit& operator=(const scope_exit& other) = delete;

        void end_now();

    private:
        std::function<void()> func;
    };
}

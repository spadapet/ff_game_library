#pragma once

namespace ff
{
    class end_scope_action
    {
    public:
        end_scope_action(std::function<void()>&& func);
        end_scope_action(end_scope_action&& other) noexcept = default;
        end_scope_action(const end_scope_action& other) = delete;
        ~end_scope_action();

        end_scope_action& operator=(end_scope_action&& other) noexcept = default;
        end_scope_action& operator=(const end_scope_action& other) = delete;

        void end_now();

    private:
        std::function<void()> func;
    };
}

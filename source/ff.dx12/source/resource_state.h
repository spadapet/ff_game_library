#pragma once

namespace ff::dx12
{
    class resource_state
    {
    public:
        enum class type_t
        {
            none, // the state wasn't set
            global, // the state outside of any executing command lists
            pending, // the first state change within a command list, not resolved yet
            promoted, // the first state change within a command list, resolved as promoted
            decayed, // the last command list is done and the state decayed to the common state
            barrier, // the first state change resolved to a barrier, or a second state change was needed
        };

        using state_t = typename std::pair<D3D12_RESOURCE_STATES, type_t>;

        resource_state() = default;
        resource_state(D3D12_RESOURCE_STATES state, type_t type, size_t array_size, size_t mip_size);
        resource_state(resource_state&& other) noexcept = default;
        resource_state(const resource_state& other) = default;

        resource_state& operator=(resource_state&& other) noexcept = default;
        resource_state& operator=(const resource_state & other) = default;

        bool all_same();
        size_t sub_resource_size() const;
        size_t array_size() const;
        size_t mip_size() const;

        void set(D3D12_RESOURCE_STATES state, type_t type, size_t array_start, size_t array_size, size_t mip_start, size_t mip_size);
        void set(D3D12_RESOURCE_STATES state, type_t type, size_t sub_resource_index, size_t sub_resource_size);
        state_t get(size_t array_index, size_t mip_index, resource_state* fallback_state = nullptr);
        state_t get(size_t sub_resource_index, resource_state* fallback_state = nullptr);
        void merge(resource_state& other);

    private:
        ff::stack_vector<state_t, 8> states;
        size_t array_size_{};
        size_t mip_size_{};
        bool check_all_same_{};
    };
}

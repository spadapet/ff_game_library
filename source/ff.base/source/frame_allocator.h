#pragma once

namespace ff
{
    class frame_allocator
    {
    public:
        frame_allocator(size_t size = 0);
        frame_allocator(frame_allocator&& other) noexcept = default;
        frame_allocator(const frame_allocator& other) = delete;

        frame_allocator& operator=(frame_allocator&& other) noexcept = default;
        frame_allocator& operator=(const frame_allocator& other) = delete;

        void* alloc(size_t size, size_t align);
        void clear();

        template<class T>
        T* alloc(size_t count = 1)
        {
            return reinterpret_cast<T*>(this->alloc(sizeof(T) * count, alignof(T)));
        }

        template<class T, class... Args>
        T* emplace(Args&&... args)
        {
            static_assert(std::is_trivially_destructible_v<T>);
            return ::new(this->alloc<T>()) T(std::forward<Args>(args)...);
        }

    private:
        struct buffer_t
        {
            std::unique_ptr<uint8_t[]> data;
            uint8_t* end{};
            uint8_t* pos{};
        };

        std::vector<buffer_t> temp_buffers;
        buffer_t buffer;
    };
}

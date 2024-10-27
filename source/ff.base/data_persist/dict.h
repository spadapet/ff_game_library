#pragma once

#include "../types/push_back.h"
#include "../data_value/value.h"

namespace ff
{
    class reader_base;
    class writer_base;

    class dict
    {
    public:
        using map_type = typename std::unordered_map<std::string_view, value_ptr>;
        using iterator = typename map_type::iterator;
        using const_iterator = typename map_type::const_iterator;

        dict() = default;
        dict(const dict& other) = default;
        dict(dict&& other) noexcept = default;

        dict& operator=(const dict& other) = default;
        dict& operator=(dict&& other) = default;
        bool operator==(const dict& other) const;
        std::ostream& operator<<(std::ostream& ostream) const;

        bool empty() const;
        size_t size() const;
        void reserve(size_t count);
        void clear();

        void set(const dict& other, bool merge_child_dicts);
        void set(std::string_view name, const value* value);
        void set_bytes(std::string_view name, const void* data, size_t size);
        value_ptr get(std::string_view name) const;
        bool get_bytes(std::string_view name, void* data, size_t size) const;

        struct location_t
        {
            std::string_view name;
            size_t offset;
            size_t size;
        };

        std::vector<std::string_view> child_names(bool sorted = false) const;
        bool save(ff::writer_base& writer, ff::push_base<ff::dict::location_t>* saved_locations = nullptr) const;
        static bool load(ff::reader_base& reader, ff::dict& data);
        bool load_child_dicts();

        iterator begin();
        const_iterator begin() const;
        const_iterator cbegin() const;

        iterator end();
        const_iterator end() const;
        const_iterator cend() const;

        void print(std::ostream& output) const;
        void debug_print() const;

        template<class T, typename... Args>
        void set(std::string_view name, Args&&... args)
        {
            this->set(name, ff::value::create<T>(std::forward<Args>(args)...));
        }

        template<class T>
        auto get(std::string_view name) const -> typename ff::type::value_traits<T>::raw_type
        {
            return this->get(name)->convert_or_default<T>()->get<T>();
        }

        template<class T, typename... Args>
        auto get(std::string_view name, Args&&... default_value_args) const -> typename ff::type::value_traits<T>::raw_type
        {
            value_ptr value = this->get(name)->try_convert<T>();
            if (!value)
            {
                value = ff::value::create<T>(std::forward<Args>(default_value_args)...);
            }

            return value->get<T>();
        }

        template<class T>
        void set_struct(std::string_view name, const T& value)
        {
            this->set_bytes(name, &value, sizeof(T));
        }

        template<class T>
        bool get_struct(std::string_view name, T& value) const
        {
            return this->get_bytes(name, &value, sizeof(T));
        }

        template<class T, class = std::enable_if_t<std::is_enum_v<T>>>
        void set_enum(std::string_view name, T value)
        {
            static_assert(sizeof(T) <= sizeof(int));
            this->set<int>(name, static_cast<int>(value));
        }

        template<class T, class = std::enable_if_t<std::is_enum_v<T>>>
        T get_enum(std::string_view name, T default_value = T{}) const
        {
            static_assert(sizeof(T) <= sizeof(int));
            return static_cast<T>(this->get<int>(name, static_cast<int>(default_value)));
        }

    private:
        value_ptr get_by_path(std::string_view path) const;

        map_type map;
    };
}

namespace std
{
    std::ostream& operator<<(std::ostream& ostream, const ff::dict& value);
}

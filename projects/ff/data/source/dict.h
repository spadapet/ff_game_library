#pragma once

#include "value_ptr.h"

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

        bool empty() const;
        size_t size() const;
        void reserve(size_t count);
        void clear();

        void set(std::string_view name, const value* value);
        void set(const dict& other, bool merge_child_dicts);
        value_ptr get(std::string_view name) const;

        std::vector<std::string_view> child_names() const;
        bool save(writer_base& writer) const;
        static bool load(reader_base& reader, dict& data);
        void load_child_dicts();

        iterator begin();
        const_iterator begin() const;
        const_iterator cbegin() const;

        iterator end();
        const_iterator end() const;
        const_iterator cend() const;

        void print(std::ostream& output) const;
        void debug_print() const;

    private:
        value_ptr get_by_path(std::string_view path) const;

        map_type map;
    };
}

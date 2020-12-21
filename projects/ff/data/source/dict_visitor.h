#pragma once

#include "value_ptr.h"

namespace ff
{
    class dict;

    class dict_visitor_base
    {
    public:
        virtual ~dict_visitor_base();

        ff::value_ptr visit_dict(const ff::dict& dict, std::vector<std::string>& errors);

    protected:
        dict_visitor_base();

        std::string path() const;
        void push_path(std::string_view name);
        void pop_path();
        bool is_root() const;

        virtual ff::value_ptr transform_dict(const ff::dict& dict);
        virtual ff::value_ptr transform_dict_async(const ff::dict& dict);
        virtual ff::value_ptr transform_vector(const std::vector<ff::value_ptr>& values);
        virtual ff::value_ptr transform_value(ff::value_ptr value);
        virtual ff::value_ptr transform_root_value(ff::value_ptr value);
        virtual void add_error(std::string_view text);

        virtual bool async_allowed(const ff::dict& dict);
        virtual void async_thread_started(DWORD main_thread_id);
        virtual void async_thread_done();

    private:
        mutable std::recursive_mutex mutex;
        std::vector<std::string> errors;
        std::unordered_map<DWORD, std::vector<std::string>> thread_to_path;
    };
}

#pragma once

namespace ff
{
    class key_frames
    {
    public:
        key_frames(const key_frames& other) = default;
        key_frames(key_frames&& other) noexcept = default;

        ff::value_ptr get_value(float frame, const ff::dict* params = nullptr);
        float start() const;
        float length() const;
        const std::string& name() const;

        static key_frames load_from_source(std::string_view name, const ff::dict& dict, ff::resource_load_context& context);
        static key_frames load_from_cache(const ff::dict& dict);
        ff::dict save_to_cache() const;

        enum class method_t
        {
            none = 0x0,

            bounds_none = 0x01,
            bounds_loop = 0x02,
            bounds_clamp = 0x04,
            bounds_bits = 0x0F,

            interpolate_linear = 0x10,
            interpolate_spline = 0x20,
            interpolate_bits = 0xF0,

            default = bounds_none | interpolate_linear,
        };

        static method_t load_method(const ff::dict& dict, bool from_cache);
        static bool adjust_frame(float& frame, float start, float length, method_t method);

    private:
        struct key_frame
        {
            bool operator<(const key_frame& other) const;
            bool operator<(float frame) const;

            float frame;
            ff::value_ptr value;
            ff::value_ptr tangent_value;
        };

        key_frames();
        bool load_from_cache_internal(const ff::dict& dict);
        bool load_from_source_internal(std::string_view name, const ff::dict& dict, ff::resource_load_context& context);
        ff::value_ptr interpolate(const key_frame& lhs, const key_frame& other, float time, const ff::dict* params);

        std::string name_;
        std::vector<key_frame> keys;
        ff::value_ptr default_value;
        float start_;
        float length_;
        method_t method;
    };

    class create_key_frames
    {
    public:
        create_key_frames(std::string_view name, float start, float length, key_frames::method_t method = key_frames::method_t::default, ff::value_ptr default_value = nullptr);
        create_key_frames(const create_key_frames& other) = default;
        create_key_frames(create_key_frames&& other) noexcept = default;

        void add_frame(float frame, ff::value_ptr value);
        key_frames create() const;
        ff::dict create_source_dict(std::string& name) const;

    private:
        ff::dict dict;
        std::vector<ff::value_ptr> values;
    };

}

#pragma once

namespace ff
{
    class input_vk;

    struct input_event_def
    {
        size_t event_id;
        double hold_seconds;
        double repeat_seconds;
        std::array<int, 4> vk;
    };

    struct input_value_def
    {
        size_t value_id;
        int vk;
    };

    class input_mapping_def
    {
    public:
        virtual ~input_mapping_def() = default;

        virtual const std::vector<input_event_def>& events() const = 0;
        virtual const std::vector<input_value_def>& values() const = 0;
    };

    struct input_event
    {
        size_t event_id;
        size_t count;

        bool started() const;
        bool repeated() const;
        bool stopped() const;
    };

    class input_event_provider
    {
    public:
        input_event_provider(const input_mapping_def& mapping, std::vector<ff::input_vk const*>&& devices);

        bool update(double delta_time = ff::constants::seconds_per_update<double>());

        const std::vector<input_event>& events() const;
        float event_progress(size_t event_id) const; // 1=triggered once, 2=hold time hit twice, etc...
        bool event_hit(size_t event_id) const;
        bool event_stopped(size_t event_id) const;

        bool digital_value(size_t value_id) const;
        float analog_value(size_t value_id) const; // 0.0f - 1.0f

    private:
        struct input_event_progress : public input_event_def
        {
            double holding_seconds;
            size_t event_count;
            bool holding;
        };

        int get_press_count(int vk) const;
        bool get_digital_value(int vk) const;
        float get_analog_value(int vk) const;
        void push_start_event(input_event_progress& event);
        void push_stop_event(input_event_progress& event);

        std::unordered_multimap<size_t, input_event_progress, ff::no_hash<size_t>> event_id_to_progress;
        std::unordered_multimap<size_t, int, ff::no_hash<size_t>> value_id_to_vk;
        std::vector<input_event> events_;
        std::vector<ff::input_vk const*> devices;
    };

    class input_mapping
        : public ff::input_mapping_def
        , public ff::resource_object_base
    {
    public:
        input_mapping(std::vector<input_event_def>&& events, std::vector<input_value_def>&& values);

        virtual const std::vector<input_event_def>& events() const override;
        virtual const std::vector<input_value_def>& values() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict) const override;

    private:
        std::vector<input_event_def> events_;
        std::vector<input_value_def> values_;
    };
}

namespace ff::internal
{
    class input_mapping_factory : public ff::resource_object_factory<ff::input_mapping>
    {
    public:
        using ff::resource_object_factory<ff::input_mapping>::resource_object_factory;

        virtual std::shared_ptr<ff::resource_object_base> load_from_source(const ff::dict& dict, ff::resource_load_context& context) const override;
        virtual std::shared_ptr<ff::resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}

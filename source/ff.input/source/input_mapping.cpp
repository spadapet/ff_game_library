#include "pch.h"
#include "input_mapping.h"
#include "input_vk.h"

bool ff::input_event::started() const
{
    return this->count == 1;
}

bool ff::input_event::repeated() const
{
    return this->count > 1;
}

bool ff::input_event::stopped() const
{
    return this->count == 0;
}

ff::input_event_provider::input_event_provider(const input_mapping_def& mapping, std::vector<input_vk const*>&& devices)
    : devices(std::move(devices))
{
    this->event_id_to_progress.reserve(mapping.events().size());
    this->value_id_to_vk.reserve(mapping.values().size());

    for (const ff::input_event_def& event_def : mapping.events())
    {
        input_event_progress event_progress{};
        static_cast<ff::input_event_def&>(event_progress) = event_def;
        this->event_id_to_progress.insert(std::make_pair(event_def.event_id, event_progress));
    }

    for (const ff::input_value_def& value_def : mapping.values())
    {
        this->value_id_to_vk.insert(std::make_pair(value_def.value_id, value_def.vk));
    }
}

bool ff::input_event_provider::advance(double delta_time)
{
    this->events_.clear();

    for (auto& pair : this->event_id_to_progress)
    {
        size_t event_id = pair.first;
        input_event_progress& event_progress = pair.second;

        bool still_holding = true;
        int trigger_count = 0;

        // Check each required button that needs to be pressed to trigger the current action
        for (int vk : event_progress.vk)
        {
            if (vk)
            {
                if (!this->get_digital_value(vk))
                {
                    still_holding = false;
                    trigger_count = 0;
                }

                int cur_trigger_count = this->get_press_count(vk);
                if (still_holding && cur_trigger_count)
                {
                    trigger_count = std::max(trigger_count, cur_trigger_count);
                }
            }
        }

        for (int h = 0; h < trigger_count; h++)
        {
            if (event_progress.event_count > 0)
            {
                this->push_stop_event(event_progress);
            }

            event_progress.holding = true;

            if (delta_time / trigger_count >= event_progress.hold_seconds)
            {
                this->push_start_event(event_progress);
            }
        }

        if (event_progress.holding)
        {
            if (!still_holding)
            {
                this->push_stop_event(event_progress);
            }
            else if (!trigger_count)
            {
                event_progress.holding_seconds += delta_time;

                double hold_time = (event_progress.holding_seconds - event_progress.hold_seconds);

                if (hold_time >= 0)
                {
                    size_t total_events = 1;

                    if (event_progress.repeat_seconds > 0)
                    {
                        total_events += static_cast<size_t>(std::floor(hold_time / event_progress.repeat_seconds));
                    }

                    while (event_progress.event_count < total_events)
                    {
                        this->push_start_event(event_progress);
                    }
                }
            }
        }
    }

    return !this->events_.empty();
}

const std::vector<ff::input_event>& ff::input_event_provider::events() const
{
    return this->events_;
}

float ff::input_event_provider::event_progress(size_t event_id) const
{
    // Can only return one value, so choose the largest
    double max_progress = 0.0;

    auto range = this->event_id_to_progress.equal_range(event_id);
    for (auto i = range.first; i != range.second; i++)
    {
        const input_event_progress& event = i->second;
        if (event.holding)
        {
            double progress = 1.0;

            if (event.holding_seconds < event.hold_seconds)
            {
                // from 0 to 1
                progress = event.holding_seconds / event.hold_seconds;
            }
            else if (event.repeat_seconds > 0.0)
            {
                // from 1 to N, using repeat count
                progress = (event.holding_seconds - event.hold_seconds) / event.repeat_seconds + 1.0;
            }
            else if (event.hold_seconds > 0.0)
            {
                // from 1 to N, using the original hold time as the repeat time
                progress = event.holding_seconds / event.hold_seconds;
            }

            max_progress = std::max(progress, max_progress);
        }
    }

    return static_cast<float>(max_progress);
}

bool ff::input_event_provider::event_hit(size_t event_id) const
{
    for (const ff::input_event& event : this->events_)
    {
        if (event.event_id == event_id && event.count >= 1)
        {
            return true;
        }
    }

    return false;
}

bool ff::input_event_provider::event_stopped(size_t event_id) const
{
    for (const ff::input_event& event : this->events_)
    {
        if (event.event_id == event_id && event.count == 0)
        {
            return true;
        }
    }

    return false;
}

bool ff::input_event_provider::digital_value(size_t value_id) const
{
    auto range = this->value_id_to_vk.equal_range(value_id);
    for (auto i = range.first; i != range.second; i++)
    {
        if (this->get_digital_value(i->second))
        {
            return true;
        }
    }

    return false;
}

float ff::input_event_provider::analog_value(size_t value_id) const
{
    float max_val = 0.0f;

    auto range = this->value_id_to_vk.equal_range(value_id);
    for (auto i = range.first; i != range.second; i++)
    {
        float val = this->get_analog_value(i->second);
        if (std::abs(val) > std::abs(max_val))
        {
            max_val = val;
        }
    }

    return max_val;
}

int ff::input_event_provider::get_press_count(int vk) const
{
    int press_count = 0;

    for (ff::input_vk const* device : this->devices)
    {
        press_count += device->press_count(vk);
    }

    return press_count;
}

bool ff::input_event_provider::get_digital_value(int vk) const
{
    for (ff::input_vk const* device : this->devices)
    {
        if (device->pressing(vk))
        {
            return true;
        }
    }

    return false;
}

float ff::input_event_provider::get_analog_value(int vk) const
{
    float max_val = 0.0f;

    for (ff::input_vk const* device : this->devices)
    {
        float val = device->analog_value(vk);
        if (std::abs(val) > std::abs(max_val))
        {
            max_val = val;
        }
    }

    return max_val;
}

void ff::input_event_provider::push_start_event(input_event_progress& event)
{
    this->events_.push_back(ff::input_event{ event.event_id, ++event.event_count });
}

void ff::input_event_provider::push_stop_event(input_event_progress& event)
{
    bool pushed_start = event.event_count > 0;

    event.holding_seconds = 0.0;
    event.event_count = 0;
    event.holding = false;

    if (pushed_start)
    {
        this->events_.push_back(ff::input_event{ event.event_id, 0 });
    }
}

ff::input_mapping::input_mapping(std::vector<input_event_def>&& events, std::vector<input_value_def>&& values)
    : events_(std::move(events))
    , values_(std::move(values))
{}

const std::vector<ff::input_event_def>& ff::input_mapping::events() const
{
    return this->events_;
}

const std::vector<ff::input_value_def>& ff::input_mapping::values() const
{
    return this->values_;
}

bool ff::input_mapping::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    dict.set<size_t>("events_size", this->events_.size());
    dict.set_bytes("events", this->events_.data(), ff::vector_byte_size(this->events_));

    dict.set<size_t>("values_size", this->values_.size());
    dict.set_bytes("values", this->values_.data(), ff::vector_byte_size(this->values_));

    return true;
}

static int name_to_vk(std::string_view name)
{
    static std::unordered_map<std::string_view, int> name_to_vk
    {
        { "mouse_left", VK_LBUTTON },
        { "mouse_right", VK_RBUTTON },
        { "mouse_middle", VK_MBUTTON },
        { "mouse_x1", VK_XBUTTON1 },
        { "mouse_x2", VK_XBUTTON2 },
        { "backspace", VK_BACK },
        { "tab", VK_TAB },
        { "return", VK_RETURN },
        { "shift", VK_SHIFT },
        { "control", VK_CONTROL },
        { "alt", VK_MENU },
        { "escape", VK_ESCAPE },
        { "space", VK_SPACE },
        { "page_up", VK_PRIOR },
        { "page_down", VK_NEXT },
        { "end", VK_END },
        { "home", VK_HOME },
        { "left", VK_LEFT },
        { "up", VK_UP },
        { "right", VK_RIGHT },
        { "down", VK_DOWN },
        { "print_screen", VK_SNAPSHOT },
        { "insert", VK_INSERT },
        { "delete", VK_DELETE },
        { "0", '0' },
        { "1", '1' },
        { "2", '2' },
        { "3", '3' },
        { "4", '4' },
        { "5", '5' },
        { "6", '6' },
        { "7", '7' },
        { "8", '8' },
        { "9", '9' },
        { "a", 'A' },
        { "b", 'B' },
        { "c", 'C' },
        { "d", 'D' },
        { "e", 'E' },
        { "f", 'F' },
        { "g", 'G' },
        { "h", 'H' },
        { "i", 'I' },
        { "j", 'J' },
        { "k", 'K' },
        { "l", 'L' },
        { "m", 'M' },
        { "n", 'N' },
        { "o", 'O' },
        { "p", 'P' },
        { "q", 'Q' },
        { "r", 'R' },
        { "s", 'S' },
        { "t", 'T' },
        { "u", 'U' },
        { "v", 'V' },
        { "w", 'W' },
        { "x", 'X' },
        { "y", 'Y' },
        { "z", 'Z' },
        { "A", 'A' },
        { "B", 'B' },
        { "C", 'C' },
        { "D", 'D' },
        { "E", 'E' },
        { "F", 'F' },
        { "G", 'G' },
        { "H", 'H' },
        { "I", 'I' },
        { "J", 'J' },
        { "K", 'K' },
        { "L", 'L' },
        { "M", 'M' },
        { "N", 'N' },
        { "O", 'O' },
        { "P", 'P' },
        { "Q", 'Q' },
        { "R", 'R' },
        { "S", 'S' },
        { "T", 'T' },
        { "U", 'U' },
        { "V", 'V' },
        { "W", 'W' },
        { "X", 'X' },
        { "Y", 'Y' },
        { "Z", 'Z' },
        { "left_win", VK_LWIN },
        { "right_win", VK_RWIN },
        { "num_0", VK_NUMPAD0 },
        { "num_1", VK_NUMPAD1 },
        { "num_2", VK_NUMPAD2 },
        { "num_3", VK_NUMPAD3 },
        { "num_4", VK_NUMPAD4 },
        { "num_5", VK_NUMPAD5 },
        { "num_6", VK_NUMPAD6 },
        { "num_7", VK_NUMPAD7 },
        { "num_8", VK_NUMPAD8 },
        { "num_9", VK_NUMPAD9 },
        { "multiply", VK_MULTIPLY },
        { "add", VK_ADD },
        { "subtract", VK_SUBTRACT },
        { "decimal", VK_DECIMAL },
        { "divide", VK_DIVIDE },
        { "f1", VK_F1 },
        { "f2", VK_F2 },
        { "f3", VK_F3 },
        { "f4", VK_F4 },
        { "f5", VK_F5 },
        { "f6", VK_F6 },
        { "f7", VK_F7 },
        { "f8", VK_F8 },
        { "f9", VK_F9 },
        { "f10", VK_F10 },
        { "f11", VK_F11 },
        { "f12", VK_F12 },
        { "left_shift", VK_LSHIFT },
        { "right_shift", VK_RSHIFT },
        { "left_control", VK_LCONTROL },
        { "right_control", VK_RCONTROL },
        { "left_alt", VK_LMENU },
        { "right_alt", VK_RMENU },
        { "colon", VK_OEM_1 },
        { "plus", VK_OEM_PLUS },
        { "comma", VK_OEM_COMMA },
        { "minus", VK_OEM_MINUS },
        { "period", VK_OEM_PERIOD },
        { "question", VK_OEM_2 },
        { "tilde", VK_OEM_3 },
        { "gamepad_a", VK_GAMEPAD_A },
        { "gamepad_b", VK_GAMEPAD_B },
        { "gamepad_x", VK_GAMEPAD_X },
        { "gamepad_y", VK_GAMEPAD_Y },
        { "gamepad_right_bumper", VK_GAMEPAD_RIGHT_SHOULDER },
        { "gamepad_left_bumper", VK_GAMEPAD_LEFT_SHOULDER },
        { "gamepad_left_trigger", VK_GAMEPAD_LEFT_TRIGGER },
        { "gamepad_right_trigger", VK_GAMEPAD_RIGHT_TRIGGER },
        { "gamepad_dpad_up", VK_GAMEPAD_DPAD_UP },
        { "gamepad_dpad_down", VK_GAMEPAD_DPAD_DOWN },
        { "gamepad_dpad_left", VK_GAMEPAD_DPAD_LEFT },
        { "gamepad_dpad_right", VK_GAMEPAD_DPAD_RIGHT },
        { "gamepad_start", VK_GAMEPAD_MENU },
        { "gamepad_back", VK_GAMEPAD_VIEW },
        { "gamepad_left_thumb", VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON },
        { "gamepad_right_thumb", VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON },
        { "gamepad_left_up", VK_GAMEPAD_LEFT_THUMBSTICK_UP },
        { "gamepad_left_down", VK_GAMEPAD_LEFT_THUMBSTICK_DOWN },
        { "gamepad_left_right", VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT },
        { "gamepad_left_left", VK_GAMEPAD_LEFT_THUMBSTICK_LEFT },
        { "gamepad_right_up", VK_GAMEPAD_RIGHT_THUMBSTICK_UP },
        { "gamepad_right_down", VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN },
        { "gamepad_right_right", VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT },
        { "gamepad_right_left", VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT },
        { "open_curly", VK_OEM_4 },
        { "pipe", VK_OEM_5 },
        { "close_curly", VK_OEM_6 },
        { "quote", VK_OEM_7 },
    };

    auto i = name_to_vk.find(name);
    return (i != name_to_vk.cend()) ? i->second : 0;
}

static bool parse_event_def(std::string_view name, const ff::dict& dict, std::vector<ff::input_event_def>& defs)
{
    ff::input_event_def def{};
    def.event_id = ff::stable_hash_func(name);
    def.hold_seconds = dict.get<double>("hold");
    def.repeat_seconds = dict.get<double>("repeat");

    std::vector<int> vks;
    ff::value_ptr action_value = dict.get("action");

    if (action_value->is_type<std::string>())
    {
        std::string vk_names_str = action_value->get<std::string>();
        std::vector<std::string_view> vk_names = ff::string::split(vk_names_str, " +");

        for (std::string_view vk_name : vk_names)
        {
            int vk = ::name_to_vk(action_value->get<std::string>());
            assert_ret_val(vk, false);
            vks.push_back(vk);
        }
    }
    else if (action_value->is_type<std::vector<ff::value_ptr>>())
    {
        for (ff::value_ptr single_action_value : action_value->get<std::vector<ff::value_ptr>>())
        {
            int vk = ::name_to_vk(single_action_value->get<std::string>());
            assert_ret_val(vk, false);
            vks.push_back(vk);
        }
    }

    if (vks.empty() || vks.size() > def.vk.size())
    {
        return false;
    }

    std::memcpy(def.vk.data(), vks.data(), ff::vector_byte_size(vks));
    defs.push_back(def);
    return true;
}

static bool parse_event_defs(const ff::dict& dict, std::vector<ff::input_event_def>& defs)
{
    for (std::string_view name : dict.child_names())
    {
        ff::value_ptr value = dict.get(name);

        if (value->is_type<std::string>())
        {
            ff::dict def_dict;
            def_dict.set<std::string>("action", value->get<std::string>());
            if (!::parse_event_def(name, def_dict, defs))
            {
                return false;
            }
        }
        else if (value->is_type<ff::dict>())
        {
            if (!::parse_event_def(name, value->get<ff::dict>(), defs))
            {
                return false;
            }
        }
        else if (value->is_type<std::vector<ff::value_ptr>>())
        {
            for (ff::value_ptr nested_value : value->get<std::vector<ff::value_ptr>>())
            {
                if (nested_value->is_type<std::string>())
                {
                    ff::dict def_dict;
                    def_dict.set<std::string>("action", nested_value->get<std::string>());
                    if (!::parse_event_def(name, def_dict, defs))
                    {
                        return false;
                    }
                }
                else if (!::parse_event_def(name, nested_value->get<ff::dict>(), defs))
                {
                    return false;
                }
            }
        }
        else
        {
            return false;
        }
    }

    return true;
}

static std::vector<ff::input_value_def> map_events_to_values(const std::vector<ff::input_event_def>& defs)
{
    std::vector<ff::input_value_def> result;

    for (const ff::input_event_def& def : defs)
    {
        result.push_back(ff::input_value_def{ def.event_id, def.vk[0] });
    }

    return result;
}

std::shared_ptr<ff::resource_object_base> ff::internal::input_mapping_factory::load_from_source(const ff::dict& dict, ff::resource_load_context& context) const
{
    std::vector<ff::input_event_def> event_defs, value_defs;
    if (::parse_event_defs(dict.get<ff::dict>("events"), event_defs) &&
        ::parse_event_defs(dict.get<ff::dict>("values"), value_defs))
    {
        return std::make_shared<ff::input_mapping>(std::move(event_defs), ::map_events_to_values(value_defs));
    }

    assert(false);
    return nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::input_mapping_factory::load_from_cache(const ff::dict& dict) const
{
    std::vector<ff::input_event_def> events;
    std::vector<ff::input_value_def> values;

    size_t events_size = dict.get<size_t>("events_size");
    if (events_size)
    {
        events.resize(events_size);
        if (!dict.get_bytes("events", events.data(), ff::vector_byte_size(events)))
        {
            return nullptr;
        }
    }

    size_t values_size = dict.get<size_t>("values_size");
    if (values_size)
    {
        values.resize(values_size);
        if (!dict.get_bytes("values", values.data(), ff::vector_byte_size(values)))
        {
            return nullptr;
        }
    }

    return std::make_shared<ff::input_mapping>(std::move(events), std::move(values));
}

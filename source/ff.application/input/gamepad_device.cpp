#include "pch.h"
#include "input/gamepad_device.h"
#include "input/input.h"
#include "input/input_device_event.h"

constexpr float PRESS_VALUE = 0.5625f;
constexpr float RELEASE_VALUE = 0.5f;

static size_t vk_to_index(int vk)
{
    return (vk >= VK_GAMEPAD_A)
        ? static_cast<size_t>(vk) - VK_GAMEPAD_A
        : VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT + 1;
}

static float analog_short_to_float(int16_t value)
{
    return value / ((value < 0) * 32768.0f + (value >= 0) * 32767.0f);
}

ff::gamepad_device::gamepad_device(size_t gamepad)
    : gamepad_(gamepad)
{
}

size_t ff::gamepad_device::gamepad() const
{
    return this->gamepad_;
}

void ff::gamepad_device::gamepad(size_t gamepad)
{
    this->gamepad_ = gamepad;
}

// static
void ff::gamepad_device::insert_vibration(std::vector<vibrate_t>& dest, vibrate_t value)
{
    check_ret(value.value && value.count);

    // Function to find the insertion point using binary search
    auto insert_pos = std::lower_bound(dest.begin(), dest.end(), value,
        [](const vibrate_t& a, const vibrate_t& b)
        {
            return a.value > b.value; // highest values first
        });

    if (insert_pos != dest.end() && insert_pos->value == value.value)
    {
        // Combine with existing value
        insert_pos->count = std::max(insert_pos->count, value.count);
        return;
    }

    for (auto i = dest.begin(); i != insert_pos; i++)
    {
        if (i->count >= value.count)
        {
            // No need for this new value, an existing value will last longer
            return;
        }
    }

    insert_pos = dest.insert(insert_pos, value);

    // Remove any lower values that would complete during the new higher value's lifetime
    dest.erase(std::remove_if(insert_pos + 1, dest.end(), [&](const vibrate_t& v) { return v.count <= value.count; }), dest.end());
}

void ff::gamepad_device::vibrate(float low_value, float high_value, float time)
{
    check_ret(this->connected());

    constexpr float MAX_VIBRATE_TIME = 2.0f;

    low_value = std::clamp(low_value, 0.0f, 1.0f);
    high_value = std::clamp(high_value, 0.0f, 1.0f);
    time = std::clamp(time, 0.0f, MAX_VIBRATE_TIME);

    const size_t count = static_cast<size_t>(time * ff::constants::updates_per_second<float>());
    ff::gamepad_device::insert_vibration(this->vibrate_low, { static_cast<WORD>(low_value * 65535.0f), count });
    ff::gamepad_device::insert_vibration(this->vibrate_high, { static_cast<WORD>(high_value * 65535.0f), count });

    this->set_vibration();
}

void ff::gamepad_device::vibrate_stop()
{
    this->vibrate_low.clear();
    this->vibrate_high.clear();
    this->set_vibration();
}

void ff::gamepad_device::set_vibration()
{
    XINPUT_VIBRATION vibration{};

    if (this->vibrate_low.size())
    {
        vibration.wLeftMotorSpeed = this->vibrate_low.front().value;
    }

    if (this->vibrate_high.size())
    {
        vibration.wRightMotorSpeed = this->vibrate_high.front().value;
    }

    if (vibration.wLeftMotorSpeed != this->current_vibration.wLeftMotorSpeed ||
        vibration.wRightMotorSpeed != this->current_vibration.wRightMotorSpeed)
    {
        this->current_vibration = vibration;

        if (this->connected())
        {
            ::XInputSetState(static_cast<DWORD>(this->gamepad_), &this->current_vibration);
        }
    }
}

float ff::gamepad_device::analog_value(int vk) const
{
    size_t i = ::vk_to_index(vk);
    return i < this->state.reading.values.size() ? this->state.reading.values[i] : 0.0f;
}

bool ff::gamepad_device::pressing(int vk) const
{
    size_t i = ::vk_to_index(vk);
    return i < this->state.pressing.size() && this->state.pressing[i];
}

int ff::gamepad_device::press_count(int vk) const
{
    size_t i = ::vk_to_index(vk);
    return (i < this->state.press_count.size() && this->state.press_count[i] == 1) ? 1 : 0;
}

void ff::gamepad_device::update()
{
    reading_t reading{};
    bool was_connected = this->connected_;
    if ((this->connected_ || !this->check_connected) && this->poll(reading))
    {
        this->connected_ = true;
    }
    else if (this->connected_ || !this->check_connected)
    {
        this->connected_ = false;
        this->check_connected = ff::constants::updates_per_second<int>() +
            (ff::math::random_non_negative() % ff::constants::updates_per_second<int>());
    }
    else
    {
        this->check_connected--;
    }

    check_ret(was_connected || this->connected_);

    // update state
    {
        std::scoped_lock lock(this->mutex);
        this->update_pending_state(reading);
        this->state = this->pending_state;
    }

    // Update vibration
    {
        const auto remove_completed = [](std::vector<vibrate_t>& v)
            {
                const auto update_count = [](vibrate_t& i) { if (i.count) i.count--; };
                const auto count_completed = [](const vibrate_t& i) { return !i.count; };

                std::for_each(v.begin(), v.end(), update_count);
                v.erase(std::remove_if(v.begin(), v.end(), count_completed), v.end());
            };

        remove_completed(this->vibrate_low);
        remove_completed(this->vibrate_high);

        this->set_vibration();
    }
}

void ff::gamepad_device::kill_pending()
{
    std::scoped_lock lock(this->mutex);
    this->update_pending_state(reading_t{});
    this->vibrate_stop();
}

bool ff::gamepad_device::connected() const
{
    return this->connected_;
}

bool ff::gamepad_device::poll(reading_t& reading)
{
    if (this->block_events() || !ff::internal::input::app_window_active())
    {
        return true;
    }

    XINPUT_STATE gs;
    DWORD status = ::XInputGetState(static_cast<DWORD>(this->gamepad_), &gs);
    if (status == ERROR_SUCCESS)
    {
        const XINPUT_GAMEPAD& gp = gs.Gamepad;
        WORD buttons = gp.wButtons;

        reading.values[::vk_to_index(VK_GAMEPAD_A)] = (buttons & XINPUT_GAMEPAD_A) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_B)] = (buttons & XINPUT_GAMEPAD_B) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_X)] = (buttons & XINPUT_GAMEPAD_X) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_Y)] = (buttons & XINPUT_GAMEPAD_Y) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_RIGHT_SHOULDER)] = (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_LEFT_SHOULDER)] = (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_RIGHT_TRIGGER)] = gp.bRightTrigger / 255.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_LEFT_TRIGGER)] = gp.bLeftTrigger / 255.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_DPAD_UP)] = (buttons & XINPUT_GAMEPAD_DPAD_UP) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_DPAD_DOWN)] = (buttons & XINPUT_GAMEPAD_DPAD_DOWN) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_DPAD_LEFT)] = (buttons & XINPUT_GAMEPAD_DPAD_LEFT) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_DPAD_RIGHT)] = (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_MENU)] = (buttons & XINPUT_GAMEPAD_START) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_VIEW)] = (buttons & XINPUT_GAMEPAD_BACK) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON)] = (buttons & XINPUT_GAMEPAD_LEFT_THUMB) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON)] = (buttons & XINPUT_GAMEPAD_RIGHT_THUMB) ? 1.0f : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_LEFT_THUMBSTICK_UP)] = (gp.sThumbLY > 0) ? ::analog_short_to_float(gp.sThumbLY) : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_LEFT_THUMBSTICK_DOWN)] = (gp.sThumbLY < 0) ? -::analog_short_to_float(gp.sThumbLY) : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT)] = (gp.sThumbLX > 0) ? ::analog_short_to_float(gp.sThumbLX) : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_LEFT_THUMBSTICK_LEFT)] = (gp.sThumbLX < 0) ? -::analog_short_to_float(gp.sThumbLX) : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_RIGHT_THUMBSTICK_UP)] = (gp.sThumbRY > 0) ? ::analog_short_to_float(gp.sThumbRY) : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN)] = (gp.sThumbRY < 0) ? -::analog_short_to_float(gp.sThumbRY) : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT)] = (gp.sThumbRX > 0) ? ::analog_short_to_float(gp.sThumbRX) : 0.0f;
        reading.values[::vk_to_index(VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT)] = (gp.sThumbRX < 0) ? -::analog_short_to_float(gp.sThumbRX) : 0.0f;

        return true;
    }

    return false;
}

void ff::gamepad_device::update_pending_state(const reading_t& reading)
{
    this->pending_state.reading = reading;

    for (size_t i = 0; i < reading.values.size(); i++)
    {
        if (reading.values[i] >= ::PRESS_VALUE)
        {
            this->pending_state.pressing[i] = true;
        }
        else if (reading.values[i] < ::RELEASE_VALUE)
        {
            this->pending_state.pressing[i] = false;
        }

        this->update_press_count(i);
    }
}

void ff::gamepad_device::update_press_count(size_t index)
{
    unsigned int vk = static_cast<unsigned int>(index) + VK_GAMEPAD_A;
    ff::input_device_event device_event{};

    if (this->pending_state.pressing[index])
    {
        ++this->pending_state.press_count[index];

        size_t count = this->pending_state.press_count[index];

        const size_t first_repeat = 30;
        const size_t repeat_count = 6;

        if (count == 1)
        {
            device_event = ff::input_device_event_key_press(vk, 1);
        }
        else if (count > first_repeat && (count - first_repeat) % repeat_count == 0)
        {
            size_t repeats = (count - first_repeat) / repeat_count + 1;
            device_event = ff::input_device_event_key_press(vk, static_cast<int>(repeats));
        }
    }
    else if (this->pending_state.press_count[index])
    {
        this->pending_state.press_count[index] = 0;
        device_event = ff::input_device_event_key_press(vk, 0);
    }

    if (device_event.type != ff::input_device_event_type::none)
    {
        this->device_event.notify(device_event);

        // Gamepad activity should prevent Windows from going to sleep
        ::SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    }
}

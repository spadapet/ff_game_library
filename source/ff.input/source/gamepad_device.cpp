#include "pch.h"
#include "gamepad_device.h"
#include "input.h"
#include "input_device_event.h"

static const float PRESS_VALUE = 0.5625f;
static const float RELEASE_VALUE = 0.5f;

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

ff::gamepad_device::gamepad_device(gamepad_type gamepad)
    : gamepad_(gamepad)
    , state{}
    , pending_state{}
    , check_connected(0)
    , connected_(true)
{
    ff::internal::input::add_device(this);
}

ff::gamepad_device::~gamepad_device()
{
    ff::internal::input::remove_device(this);
}

ff::gamepad_device::gamepad_type ff::gamepad_device::gamepad() const
{
    return this->gamepad_;
}

void ff::gamepad_device::gamepad(gamepad_type gamepad)
{
    this->gamepad_ = gamepad;
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

void ff::gamepad_device::advance()
{
    reading_t reading{};
    if ((this->connected_ || !this->check_connected) && this->poll(reading))
    {
        this->connected_ = true;
    }
    else if (this->connected_ || !this->check_connected)
    {
        this->connected_ = false;
        this->check_connected =
            ff::constants::advances_per_second_s +
            static_cast<size_t>(ff::math::random_non_negative()) %
            ff::constants::advances_per_second_s;
    }
    else
    {
        this->check_connected--;
    }

    std::scoped_lock lock(this->mutex);
    this->update_pending_state(reading);
    this->state = this->pending_state;
}

void ff::gamepad_device::kill_pending()
{
    std::scoped_lock lock(this->mutex);
    this->update_pending_state(reading_t{});
}

bool ff::gamepad_device::connected() const
{
    return this->connected_;
}

ff::signal_sink<const ff::input_device_event&>& ff::gamepad_device::event_sink()
{
    return this->device_event;
}

void ff::gamepad_device::notify_main_window_message(ff::window_message& message)
{}

bool ff::gamepad_device::poll(reading_t& reading)
{
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

    if (device_event.type != ff::input_device_event_type::none && ff::window::main()->focused())
    {
        this->device_event.notify(device_event);

        // Gamepad activity should prevent Windows from going to sleep
        ::SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    }
}

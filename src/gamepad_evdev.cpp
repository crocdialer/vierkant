#ifdef __linux__

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <optional>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <crocore/crocore.hpp>

// only for the canonical index-constants (GLFW_GAMEPAD_BUTTON_* / GLFW_GAMEPAD_AXIS_*),
// shared with the interpretation-layer in Input.cpp. no glfw joystick-api is used here.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "gamepad.hpp"
#include "gamepad_evdev.hpp"

namespace vierkant::gamepad
{

axis_layout_t detect_axis_layout(const unsigned long *abs_bits)
{
    if(test_bit(ABS_GAS, abs_bits) && test_bit(ABS_BRAKE, abs_bits))
    {
        return {.right_x = ABS_Z, .right_y = ABS_RZ, .trigger_left = ABS_BRAKE, .trigger_right = ABS_GAS};
    }
    return {.right_x = ABS_RX, .right_y = ABS_RY, .trigger_left = ABS_Z, .trigger_right = ABS_RZ};
}

float normalize_axis(const input_absinfo &abs, bool centered)
{
    if(abs.maximum == abs.minimum) { return 0.f; }
    const float value =
            std::clamp(static_cast<float>(abs.value), static_cast<float>(abs.minimum), static_cast<float>(abs.maximum));
    const float t = (value - static_cast<float>(abs.minimum)) /
                    (static_cast<float>(abs.maximum) - static_cast<float>(abs.minimum));
    return centered ? t * 2.f - 1.f : t;
}

bool axis_unreported(const input_absinfo &abs)
{
    // the kernel zero-initializes its axis-values, so an unreported axis reads as 0. on an unsigned
    // range that is hard-deflection, on a signed one it is the rest-position and nothing is wrong.
    return abs.minimum >= 0 && abs.value == abs.minimum;
}

namespace
{

//! evdev-codes for the canonical buttons 0..10, the d-pad is handled separately.
//! @note the kernel aliases BTN_X == BTN_NORTH and BTN_Y == BTN_WEST, which is geometrically odd for
//!       an xbox-layout. verified against xbox-pads only, other drivers might come out swapped.
constexpr std::array<int, GLFW_GAMEPAD_BUTTON_RIGHT_THUMB + 1> g_button_codes = {
        BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TR, BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR};

struct device_t
{
    int fd = -1;
    uint64_t id = 0;
    std::string node;
    std::string name;
    axis_layout_t layout = {};
    bool dpad_is_hat = false;
    bool has_rumble = false;

    //! set while the device has not reported yet - its stick-axes are a kernel-default, not a rest-position.
    bool sticks_unreported = false;

    //! id of a single uploaded FF_RUMBLE-effect, re-uploaded in place. -1 means: none allocated yet.
    int16_t effect_id = -1;
};

std::vector<device_t> g_devices;
uint64_t g_next_device_id = 1;
int g_inotify_fd = -1;
bool g_hotplug_disabled = false;
bool g_scan_pending = true;

void close_device(device_t &device)
{
    if(device.effect_id >= 0) { ioctl(device.fd, EVIOCRMFF, device.effect_id); }
    close(device.fd);
    device.fd = -1;
}

//! true if any stick still carries the kernel-default, i.e. the device has not reported yet
bool no_reports_yet(int fd, const axis_layout_t &layout)
{
    for(int code: {ABS_X, ABS_Y, layout.right_x, layout.right_y})
    {
        input_absinfo abs = {};
        if(ioctl(fd, EVIOCGABS(code), &abs) == 0 && axis_unreported(abs)) { return true; }
    }
    return false;
}

std::optional<device_t> open_device(const std::string &node, bool &access_denied)
{
    // O_RDWR: force-feedback requires write-access. read-only still yields input, so fall back to it.
    int fd = open(node.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    const bool read_write = fd >= 0;
    if(fd < 0) { fd = open(node.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC); }
    if(fd < 0)
    {
        // a node we cannot open cannot be classified either. only reported if no gamepad turns up at all.
        access_denied = access_denied || errno == EACCES;
        return {};
    }
    unsigned long key_bits[num_longs(KEY_MAX)] = {}, abs_bits[num_longs(ABS_MAX)] = {}, ff_bits[num_longs(FF_MAX)] = {};
    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);
    ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ff_bits)), ff_bits);

    // a gamepad is: two absolute axes plus the kernel's south face-button. no GUID, no database.
    if(!(test_bit(ABS_X, abs_bits) && test_bit(ABS_Y, abs_bits) && test_bit(BTN_SOUTH, key_bits)))
    {
        close(fd);
        return {};
    }
    char name[256] = {};
    if(ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 0) { strcpy(name, "unknown gamepad"); }

    device_t device = {};
    device.fd = fd;
    device.id = g_next_device_id++;
    device.node = node;
    device.name = name;
    device.layout = detect_axis_layout(abs_bits);
    device.dpad_is_hat = test_bit(ABS_HAT0X, abs_bits);
    device.has_rumble = read_write && test_bit(FF_RUMBLE, ff_bits);
    device.sticks_unreported = no_reports_yet(fd, device.layout);

    if(!read_write && test_bit(FF_RUMBLE, ff_bits))
    {
        spdlog::warn("gamepad '{}' ({}) could only be opened read-only - no force-feedback", device.name, device.node);
    }
    spdlog::debug("gamepad connected: '{}' ({}, rumble: {})", device.name, device.node,
                  device.has_rumble ? "yes" : "no");
    return device;
}

void scan_devices()
{
    DIR *dir = opendir("/dev/input");
    if(!dir) { return; }

    std::vector<std::string> nodes;
    while(const dirent *entry = readdir(dir))
    {
        if(!strncmp(entry->d_name, "event", 5)) { nodes.push_back(std::string("/dev/input/") + entry->d_name); }
    }
    closedir(dir);

    // readdir-order is arbitrary, sorting keeps the exposed device-order stable across scans
    std::ranges::sort(nodes);

    bool access_denied = false;

    for(const auto &node: nodes)
    {
        auto same_node = [&node](const device_t &d) { return d.node == node; };
        if(std::ranges::any_of(g_devices, same_node)) { continue; }
        if(auto device = open_device(node, access_denied)) { g_devices.push_back(std::move(*device)); }
    }
}

//! watch /dev/input for hotplug. IN_ATTRIB matters as much as IN_CREATE: udev applies the ACL after creation.
void poll_hotplug()
{
    if(g_hotplug_disabled) { return; }

    if(g_inotify_fd < 0)
    {
        g_inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

        if(g_inotify_fd < 0 || inotify_add_watch(g_inotify_fd, "/dev/input", IN_CREATE | IN_ATTRIB | IN_DELETE) < 0)
        {
            spdlog::warn("could not watch /dev/input ({}) - gamepad-hotplug is disabled", strerror(errno));
            if(g_inotify_fd >= 0) { close(g_inotify_fd); }
            g_inotify_fd = -1;
            g_hotplug_disabled = true;
            return;
        }
    }
    alignas(inotify_event) char buf[4096];

    for(ssize_t num_bytes; (num_bytes = read(g_inotify_fd, buf, sizeof(buf))) > 0;)
    {
        for(ssize_t i = 0; i < num_bytes;)
        {
            const auto *event = reinterpret_cast<const inotify_event *>(buf + i);
            if(event->len && !strncmp(event->name, "event", 5)) { g_scan_pending = true; }
            i += static_cast<ssize_t>(sizeof(inotify_event) + event->len);
        }
    }
}

//! drain the device's event-queue. we poll state via ioctl, but an unread queue grows and hides device-loss.
bool drain_events(device_t &device)
{
    input_event events[32];
    for(;;)
    {
        ssize_t num_bytes = read(device.fd, events, sizeof(events));

        // an EV_ABS means the kernel has seen a report, so its axis-values are real from here on
        for(ssize_t i = 0; device.sticks_unreported && i < num_bytes / static_cast<ssize_t>(sizeof(input_event)); ++i)
        {
            if(events[i].type == EV_ABS) { device.sticks_unreported = false; }
        }
        if(num_bytes < 0) { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
        if(num_bytes < static_cast<ssize_t>(sizeof(events))) { return true; }
    }
}

bool read_state(const device_t &device, state_t &out)
{
    unsigned long key_bits[num_longs(KEY_MAX)] = {};
    if(ioctl(device.fd, EVIOCGKEY(sizeof(key_bits)), key_bits) < 0) { return false; }

    auto read_axis = [&device](int code, bool centered) -> float {
        input_absinfo abs = {};
        if(ioctl(device.fd, EVIOCGABS(code), &abs) < 0) { return 0.f; }
        return normalize_axis(abs, centered);
    };

    out.axis.resize(GLFW_GAMEPAD_AXIS_LAST + 1);
    out.axis[GLFW_GAMEPAD_AXIS_LEFT_X] = read_axis(ABS_X, true);
    out.axis[GLFW_GAMEPAD_AXIS_LEFT_Y] = read_axis(ABS_Y, true);
    out.axis[GLFW_GAMEPAD_AXIS_RIGHT_X] = read_axis(device.layout.right_x, true);
    out.axis[GLFW_GAMEPAD_AXIS_RIGHT_Y] = read_axis(device.layout.right_y, true);

    if(device.sticks_unreported)
    {
        out.axis[GLFW_GAMEPAD_AXIS_LEFT_X] = out.axis[GLFW_GAMEPAD_AXIS_LEFT_Y] = 0.f;
        out.axis[GLFW_GAMEPAD_AXIS_RIGHT_X] = out.axis[GLFW_GAMEPAD_AXIS_RIGHT_Y] = 0.f;
    }

    // triggers are one-sided, but the canonical layout expects them in [-1, 1]
    out.axis[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = read_axis(device.layout.trigger_left, false) * 2.f - 1.f;
    out.axis[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = read_axis(device.layout.trigger_right, false) * 2.f - 1.f;

    out.buttons.assign(GLFW_GAMEPAD_BUTTON_LAST + 1, 0);
    for(uint32_t i = 0; i < g_button_codes.size(); ++i)
    {
        out.buttons[i] = test_bit(g_button_codes[i], key_bits) ? 1 : 0;
    }

    // the d-pad is either a hat-axis or four buttons
    int dpad_x = 0, dpad_y = 0;
    if(device.dpad_is_hat)
    {
        input_absinfo abs = {};
        if(ioctl(device.fd, EVIOCGABS(ABS_HAT0X), &abs) == 0) { dpad_x = abs.value; }
        if(ioctl(device.fd, EVIOCGABS(ABS_HAT0Y), &abs) == 0) { dpad_y = abs.value; }
    }
    else
    {
        auto button = [&key_bits](int code) { return static_cast<int>(test_bit(code, key_bits)); };
        dpad_x = button(BTN_DPAD_RIGHT) - button(BTN_DPAD_LEFT);
        dpad_y = button(BTN_DPAD_DOWN) - button(BTN_DPAD_UP);
    }
    out.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] = dpad_x > 0;
    out.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] = dpad_x < 0;
    out.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] = dpad_y > 0;
    out.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] = dpad_y < 0;

    out.id = device.id;
    out.name = device.name;
    return true;
}

device_t *find_device(uint64_t id)
{
    auto it = std::find_if(g_devices.begin(), g_devices.end(), [id](const device_t &d) { return d.id == id; });
    return it != g_devices.end() ? &(*it) : nullptr;
}

}// namespace

std::vector<state_t> poll_states()
{
    auto drop_device = [](std::vector<device_t>::iterator it) {
        spdlog::debug("gamepad disconnected: '{}' ({})", it->name, it->node);
        close_device(*it);
        return g_devices.erase(it);
    };

    // devices vanish mid-session, every ioctl/read must tolerate that. this runs before the scan below,
    // because a freed node-name can immediately be taken by the next device.
    for(auto it = g_devices.begin(); it != g_devices.end();) { it = drain_events(*it) ? it + 1 : drop_device(it); }

    poll_hotplug();
    if(g_scan_pending)
    {
        scan_devices();
        g_scan_pending = false;
    }

    std::vector<state_t> ret;
    ret.reserve(g_devices.size());

    for(auto it = g_devices.begin(); it != g_devices.end();)
    {
        state_t state;
        if(!read_state(*it, state))
        {
            it = drop_device(it);
            continue;
        }
        ret.push_back(std::move(state));
        ++it;
    }
    return ret;
}

bool rumble(uint64_t device_id, float strong, float weak, uint32_t duration_ms)
{
    device_t *device = find_device(device_id);
    if(!device || !device->has_rumble) { return false; }

    constexpr float max_magnitude = 0xffff;
    ff_effect effect = {};
    effect.type = FF_RUMBLE;

    // re-upload into the same slot rather than allocating an effect per call, -1 allocates the first one
    effect.id = device->effect_id;
    effect.u.rumble.strong_magnitude = static_cast<uint16_t>(std::clamp(strong, 0.f, 1.f) * max_magnitude);
    effect.u.rumble.weak_magnitude = static_cast<uint16_t>(std::clamp(weak, 0.f, 1.f) * max_magnitude);
    effect.replay.length = static_cast<uint16_t>(std::min<uint32_t>(duration_ms, 0xffff));

    if(ioctl(device->fd, EVIOCSFF, &effect) < 0)
    {
        // the slot can be invalidated behind our back, drop it so the next call allocates a fresh one
        device->effect_id = -1;
        return false;
    }
    device->effect_id = effect.id;

    input_event play = {};
    play.type = EV_FF;
    play.code = static_cast<uint16_t>(effect.id);
    play.value = 1;
    return write(device->fd, &play, sizeof(play)) == sizeof(play);
}

}// namespace vierkant::gamepad

#endif// __linux__

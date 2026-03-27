#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <thread>

#include "sim.hpp"

namespace {
std::atomic<bool> g_running{true};

constexpr int kGridSize = 24;
constexpr int kPanelSize = 8;
constexpr int kFrameRate = 30;
constexpr int kFrameTimeMs = 1000 / kFrameRate;
constexpr int kLoopFrames = 900;

using Bitmap = std::array<uint8_t, 5 * kPanelSize * kPanelSize>;
using Canvas = std::array<std::array<uint8_t, kGridSize>, kGridSize>;

enum class FormType {
    DRight,
    TriangleBottomRight,
    TriangleTopLeft,
    DUp,
};

constexpr std::array<FormType, 4> kForms = {{
    FormType::DRight,
    FormType::TriangleBottomRight,
    FormType::TriangleTopLeft,
    FormType::DUp,
}};

constexpr std::array<int, 4> kLogoTop = {{0, 0, 4, 4}};
constexpr std::array<int, 4> kLogoLeft = {{0, 4, 0, 4}};

void signal_handler(int)
{
    g_running = false;
}

int clamp_i(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

int sign_i(int value)
{
    if (value < 0) {
        return -1;
    }
    if (value > 0) {
        return 1;
    }
    return 0;
}

uint32_t hash32(int y, int x, int frame)
{
    uint32_t h = static_cast<uint32_t>(y * 73856093U) ^ static_cast<uint32_t>(x * 19349663U)
        ^ static_cast<uint32_t>(frame * 83492791U);
    h ^= h >> 13;
    h *= 1274126177U;
    return h;
}

bool is_drawable(int y, int x)
{
    const int panel_y = y / kPanelSize;
    const int panel_x = x / kPanelSize;
    return (panel_y == 0 && panel_x == 1) || (panel_y == 1 && panel_x == 0)
        || (panel_y == 1 && panel_x == 1) || (panel_y == 1 && panel_x == 2)
        || (panel_y == 2 && panel_x == 1);
}

int panel_index(int y, int x)
{
    const int panel_y = y / kPanelSize;
    const int panel_x = x / kPanelSize;

    if (panel_y == 0 && panel_x == 1) {
        return 2;
    }
    if (panel_y == 1 && panel_x == 0) {
        return 4;
    }
    if (panel_y == 1 && panel_x == 1) {
        return 1;
    }
    if (panel_y == 1 && panel_x == 2) {
        return 3;
    }
    if (panel_y == 2 && panel_x == 1) {
        return 0;
    }
    return -1;
}

void set_pixel(Canvas &canvas, int y, int x, uint8_t value)
{
    if (y < 0 || y >= kGridSize || x < 0 || x >= kGridSize) {
        return;
    }
    if (!is_drawable(y, x)) {
        return;
    }
    canvas[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = value;
}

bool form_pixel(FormType type, int local_y, int local_x)
{
    if (local_y < 0 || local_y >= 4 || local_x < 0 || local_x >= 4) {
        return false;
    }

    switch (type) {
    case FormType::DRight: {
        constexpr std::array<std::array<uint8_t, 4>, 4> pattern = {{
            {{1, 1, 1, 0}},
            {{1, 0, 0, 1}},
            {{1, 0, 0, 1}},
            {{1, 1, 1, 0}},
        }};
        return pattern[static_cast<std::size_t>(local_y)][static_cast<std::size_t>(local_x)] != 0;
    }
    case FormType::TriangleBottomRight:
        return local_x + local_y >= 3;
    case FormType::TriangleTopLeft:
        return local_x + local_y <= 3;
    case FormType::DUp: {
        constexpr std::array<std::array<uint8_t, 4>, 4> pattern = {{
            {{0, 1, 1, 0}},
            {{1, 0, 0, 1}},
            {{1, 0, 0, 1}},
            {{1, 1, 1, 1}},
        }};
        return pattern[static_cast<std::size_t>(local_y)][static_cast<std::size_t>(local_x)] != 0;
    }
    }

    return false;
}

void draw_form(Canvas &canvas, FormType type, int top, int left, int reveal_pixels)
{
    const int reveal = clamp_i(reveal_pixels, 0, 16);
    int seen = 0;

    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (!form_pixel(type, y, x)) {
                continue;
            }
            if (seen >= reveal) {
                continue;
            }
            set_pixel(canvas, top + y, left + x, HIGH);
            ++seen;
        }
    }
}

void draw_logo(Canvas &canvas, int top, int left, int reveal_pixels)
{
    for (int i = 0; i < 4; ++i) {
        draw_form(canvas, kForms[static_cast<std::size_t>(i)], top + kLogoTop[static_cast<std::size_t>(i)],
            left + kLogoLeft[static_cast<std::size_t>(i)], reveal_pixels);
    }
}

Canvas build_canvas(int frame)
{
    Canvas canvas{};
    const int phase = frame % kLoopFrames;
    constexpr std::array<int, 4> base_top = {{8, 8, 12, 12}};
    constexpr std::array<int, 4> base_left = {{8, 12, 8, 12}};

    constexpr std::array<int, 4> arm_top = {{2, 10, 18, 10}};
    constexpr std::array<int, 4> arm_left = {{10, 18, 10, 2}};

    constexpr std::array<int, 4> split_top = {{0, 8, 20, 12}};
    constexpr std::array<int, 4> split_left = {{8, 20, 8, 0}};

    if (phase < 80) {
        for (int i = 0; i < 4; ++i) {
            const int local = phase - i * 10;
            const int reveal = clamp_i((local * 16) / 36, 0, 16);
            draw_form(canvas, kForms[static_cast<std::size_t>(i)], base_top[static_cast<std::size_t>(i)],
                base_left[static_cast<std::size_t>(i)], reveal);
        }
        return canvas;
    }

    if (phase < 170) {
        const int shimmer = phase - 80;

        for (int i = 0; i < 4; ++i) {
            draw_form(canvas, kForms[static_cast<std::size_t>(i)], base_top[static_cast<std::size_t>(i)],
                base_left[static_cast<std::size_t>(i)], 16);
        }

        for (int y = 8; y < 16; ++y) {
            for (int x = 8; x < 16; ++x) {
                const uint32_t sparkle = hash32(y, x, shimmer) & 0xFFU;
                if (sparkle > 220U) {
                    set_pixel(canvas, y, x, HIGH);
                }
                if (sparkle < 12U && ((shimmer / 3) % 2 == 0)) {
                    set_pixel(canvas, y, x, LOW);
                }
            }
        }
        return canvas;
    }

    if (phase < 280) {
        const int burst = phase - 170;
        const int travel = (burst * 7) / 110;

        for (int i = 0; i < 4; ++i) {
            const int top = base_top[static_cast<std::size_t>(i)];
            const int left = base_left[static_cast<std::size_t>(i)];

            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if (!form_pixel(kForms[static_cast<std::size_t>(i)], y, x)) {
                        continue;
                    }

                    const int gy = top + y;
                    const int gx = left + x;
                    const int dy = sign_i(gy - 11);
                    const int dx = sign_i(gx - 11);
                    const int jitter = static_cast<int>(hash32(gy, gx, burst) % 2U);
                    const int d = travel + jitter;

                    set_pixel(canvas, gy + dy * d, gx + dx * d, HIGH);
                    if (burst < 45) {
                        set_pixel(canvas, gy, gx, HIGH);
                    }
                }
            }
        }

        for (int i = 0; i < 6; ++i) {
            const int sy = static_cast<int>(hash32(phase, i, 3) % 24U);
            const int sx = static_cast<int>(hash32(phase, i, 9) % 24U);
            if (is_drawable(sy, sx)) {
                set_pixel(canvas, sy, sx, HIGH);
            }
        }
        return canvas;
    }

    if (phase < 410) {
        const int rebuild = phase - 280;
        const int orbit = (rebuild / 20) % 4;
        const int step = rebuild % 20;
        const int weight = step <= 10 ? step : 20 - step;

        for (int i = 0; i < 4; ++i) {
            const int target = (i + orbit) % 4;
            const int start_top = base_top[static_cast<std::size_t>(i)];
            const int start_left = base_left[static_cast<std::size_t>(i)];
            const int end_top = arm_top[static_cast<std::size_t>(target)];
            const int end_left = arm_left[static_cast<std::size_t>(target)];

            const int current_top = start_top + ((end_top - start_top) * weight) / 10;
            const int current_left = start_left + ((end_left - start_left) * weight) / 10;

            draw_form(canvas, kForms[static_cast<std::size_t>(i)], current_top, current_left, 16);
        }
        return canvas;
    }

    if (phase < 500) {
        const int breathe = phase - 410;
        const int pulse = (breathe % 30);
        const int expand = pulse <= 15 ? pulse / 5 : (30 - pulse) / 5;

        for (int i = 0; i < 4; ++i) {
            const int top = base_top[static_cast<std::size_t>(i)];
            const int left = base_left[static_cast<std::size_t>(i)];

            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if (!form_pixel(kForms[static_cast<std::size_t>(i)], y, x)) {
                        continue;
                    }
                    const int gy = top + y;
                    const int gx = left + x;
                    const int dy = sign_i(gy - 11);
                    const int dx = sign_i(gx - 11);
                    set_pixel(canvas, gy + dy * expand, gx + dx * expand, HIGH);
                }
            }
        }

        if ((breathe / 8) % 2 == 0) {
            for (int i = 0; i < 12; ++i) {
                const int sy = static_cast<int>(hash32(phase, i, 29) % 24U);
                const int sx = static_cast<int>(hash32(phase, i, 31) % 24U);
                if (is_drawable(sy, sx)) {
                    set_pixel(canvas, sy, sx, HIGH);
                }
            }
        }
        return canvas;
    }

    if (phase < 560) {
        for (int i = 0; i < 4; ++i) {
            draw_form(canvas, kForms[static_cast<std::size_t>(i)], base_top[static_cast<std::size_t>(i)],
                base_left[static_cast<std::size_t>(i)], 16);
        }

        for (int i = 0; i < 10; ++i) {
            const int sy = static_cast<int>(hash32(phase, i, 41) % 24U);
            const int sx = static_cast<int>(hash32(phase, i, 43) % 24U);
            if (is_drawable(sy, sx) && ((phase + i) % 4 == 0)) {
                set_pixel(canvas, sy, sx, HIGH);
            }
        }
        return canvas;
    }

    if (phase < 600) {
        const int split = phase - 560;
        const int travel = clamp_i((split * 10) / 40, 0, 10);

        for (int i = 0; i < 4; ++i) {
            const int start_top = base_top[static_cast<std::size_t>(i)];
            const int start_left = base_left[static_cast<std::size_t>(i)];
            const int end_top = split_top[static_cast<std::size_t>(i)];
            const int end_left = split_left[static_cast<std::size_t>(i)];

            const int wobble_y = static_cast<int>(hash32(phase, i, 91) % 3U) - 1;
            const int wobble_x = static_cast<int>(hash32(phase, i, 99) % 3U) - 1;
            const int current_top = start_top + ((end_top - start_top) * travel) / 10 + wobble_y;
            const int current_left = start_left + ((end_left - start_left) * travel) / 10 + wobble_x;
            draw_form(canvas, kForms[static_cast<std::size_t>(i)], current_top, current_left, 16);

            set_pixel(canvas, current_top + 1 + wobble_y, current_left + 1 + wobble_x, HIGH);
        }
        return canvas;
    }

    if (phase < 660) {
        const int solo = phase - 600;

        for (int i = 0; i < 4; ++i) {
            int top = split_top[static_cast<std::size_t>(i)];
            int left = split_left[static_cast<std::size_t>(i)];

            if (i == 0) {
                top += ((solo / 6) % 3) - 1;
                left += ((solo / 8) % 3) - 1;
            } else if (i == 1) {
                top += (solo % 10 < 5) ? 0 : 1;
            } else if (i == 2) {
                left += (solo % 12 < 6) ? 0 : -1;
            } else {
                top += static_cast<int>(hash32(solo, i, 7) % 3U) - 1;
                left += static_cast<int>(hash32(solo, i, 11) % 3U) - 1;
            }

            draw_form(canvas, kForms[static_cast<std::size_t>(i)], top, left, 16);
        }
        return canvas;
    }

    if (phase < 700) {
        const int ramp = phase - 660;
        const int threshold = 240 - ramp * 5;

        for (int y = 0; y < kGridSize; ++y) {
            for (int x = 0; x < kGridSize; ++x) {
                if (!is_drawable(y, x)) {
                    continue;
                }
                const uint32_t tw = hash32(y, x, phase) & 0xFFU;
                if (tw > static_cast<uint32_t>(threshold)) {
                    set_pixel(canvas, y, x, HIGH);
                }
            }
        }
        return canvas;
    }

    if (phase < 714) {
        for (int y = 0; y < kGridSize; ++y) {
            for (int x = 0; x < kGridSize; ++x) {
                if (is_drawable(y, x)) {
                    set_pixel(canvas, y, x, HIGH);
                }
            }
        }
        return canvas;
    }

    if (phase < 860) {
        const int finale = phase - 714;
        const bool blink_off = (finale >= 64 && finale <= 67) || (finale >= 112 && finale <= 115);

        if (blink_off) {
            return canvas;
        }
        draw_logo(canvas, base_top[0], base_left[0], 16);
        return canvas;
    }

    return canvas;
}

Bitmap canvas_to_bitmap(const Canvas &canvas)
{
    Bitmap bitmap{};

    for (int y = 0; y < kGridSize; ++y) {
        for (int x = 0; x < kGridSize; ++x) {
            if (!is_drawable(y, x)) {
                continue;
            }

            const int panel = panel_index(y, x);
            if (panel < 0) {
                continue;
            }

            const int local_y = y % kPanelSize;
            const int local_x = x % kPanelSize;
            const int index = panel * 64 + local_y * 8 + local_x;
            bitmap[static_cast<std::size_t>(index)] =
                canvas[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
        }
    }

    return bitmap;
}

void send_bitmap(const Bitmap &bitmap)
{
    sim(SORTIE_3, HIGH);

    for (int panel = 4; panel >= 0; --panel) {
        for (int row = 0; row < 8; ++row) {
            for (int col = 7; col >= 0; --col) {
                const int index = panel * 64 + row * 8 + col;
                const uint8_t bit = bitmap[static_cast<std::size_t>(index)] ? HIGH : LOW;
                sim(SORTIE_4, bit);
                sim(SORTIE_2, HIGH);
            }
        }
    }

    sim(SORTIE_1, HIGH);
}
} // namespace

int main()
{
    std::signal(SIGINT, signal_handler);
    sim_init();

    int frame = 0;
    while (g_running) {
        const Canvas canvas = build_canvas(frame);
        const Bitmap bitmap = canvas_to_bitmap(canvas);

        send_bitmap(bitmap);
        std::this_thread::sleep_for(std::chrono::milliseconds(kFrameTimeMs));
        frame = (frame + 1) % kLoopFrames;
    }

    const Bitmap empty{};
    send_bitmap(empty);
    return 0;
}

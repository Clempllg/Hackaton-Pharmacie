#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

#include "sim.hpp"

namespace {
std::atomic<bool> g_running{true};

void signal_handler(int)
{
    g_running = false;
}

void set_outputs(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4)
{
    sim(SORTIE_1, o1);
    sim(SORTIE_2, o2);
    sim(SORTIE_3, o3);
    sim(SORTIE_4, o4);
}
} // namespace

int main()
{
    std::signal(SIGINT, signal_handler);
    sim_init();

    const std::array<std::array<uint8_t, 4>, 8> pattern = {{
        {{HIGH, LOW, LOW, LOW}},
        {{HIGH, HIGH, LOW, LOW}},
        {{LOW, HIGH, LOW, LOW}},
        {{LOW, HIGH, HIGH, LOW}},
        {{LOW, LOW, HIGH, LOW}},
        {{LOW, LOW, HIGH, HIGH}},
        {{LOW, LOW, LOW, HIGH}},
        {{HIGH, LOW, LOW, HIGH}},
    }};

    std::size_t step = 0;
    while (g_running) {
        const auto &state = pattern[step % pattern.size()];
        set_outputs(state[0], state[1], state[2], state[3]);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        ++step;
    }

    set_outputs(LOW, LOW, LOW, LOW);
    return 0;
}

#pragma once

#include <vector>
#include <string>

namespace config {

    // general
    constexpr int N_LIFTS = 3;
    constexpr auto STATE_SAVE_NAME = "state_save.cereal";

    // network
    const char * const IPS[] = {"129.241.187.141", "129.241.187.143", "129.241.187.146"};
    //const char * const IPS[] = {"127.0.0.1", "127.0.0.1", "127.0.0.1"};
    constexpr int STATE_PORT = 20000;

    // FS
    const int INCORRECT_DIRECTION_PENALTY = 2;
    const int POSITION_DIFFERENCE_PENALTY = 1;
    const int DISCONNECTED_PENALTY = 10 * N_LIFTS;
    const int DISCONNECTED_TIMEOUT_MS = 1000;
}

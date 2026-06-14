#include "Config.h"

static std::unique_ptr<Config> g_instance;

Config& Config::GetInstance() {
    if (!g_instance) {
        g_instance.reset(new Config());
    }
    return *g_instance;
}
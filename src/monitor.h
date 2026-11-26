#pragma once
// Робочий режим: окрема задача по черзі опитує логер (раз на LOGGER_POLL_SECONDS), шукає його, якщо пропав,
// і питає siren.pp.ua (раз на хвилину, лише з регіоном). Екран бере знімок через snapshot().
#include "settings.h"
#include "tiles.h"

namespace monitor {

void begin(const settings::Config& cfg);
tiles::Input snapshot();  // для tiles::build()

}  // namespace monitor

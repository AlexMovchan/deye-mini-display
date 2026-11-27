#pragma once
// Робочий режим: окрема задача по черзі опитує логер (раз на LOGGER_POLL_SECONDS), шукає його, якщо пропав,
// і питає siren.pp.ua (раз на хвилину, лише з регіоном). Екран бере знімок через snapshot().
#include "settings.h"
#include "outage.h"
#include "tiles.h"

namespace monitor {

void begin(const settings::Config& cfg);
tiles::Input snapshot();  // для tiles::build()
// Графік відключень: false — не налаштований. group — поточна черга (Київ може змінитися за адресою).
bool outageSnapshot(outage::Schedule* out, String* group, outage::Fetch* fetch = nullptr);

}  // namespace monitor

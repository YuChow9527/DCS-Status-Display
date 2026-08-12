#pragma once

#include <cstring>
#include "modules/apache.hpp"

// Unit conversion for the DCS display. DCS sends raw metric values (meters,
// m/s); conversion to the display unit system happens here on the ESP32.
namespace dcs_units
{

// Aircraft using the metric system (prefix match).
inline constexpr const char *METRIC_AIRCRAFT[] = {
    "Su-25", "Su-27", "Su-33",
    "MiG-15", "MiG-19", "MiG-21", "MiG-23", "MiG-29", "MiG-29 Fulcrum",
    "Ka-50", "Ka-50_3", "Mi-8", "Mi-24",
    "Yak-52", "L-39", "J-11",
    "AH-64", "SA342",
};

inline bool aircraftUsesMetric(const char *name)
{
    for (const char *p : METRIC_AIRCRAFT)
    {
        if (strncmp(name, p, strlen(p)) == 0)
            return true;
    }
    return false;
}

inline bool altitudeIsMetric(const char *name)
{
    return aircraftUsesMetric(name);
}

inline bool speedIsMetric(const char *name)
{
    return aircraftUsesMetric(name) && !apacheSpeedImperial(name);
}

inline float mToFt(float meters)
{
    return meters * 3.28084f;
}

inline float msToKts(float ms)
{
    return ms * 1.943844f;
}

inline float msToKmh(float ms)
{
    return ms * 3.6f;
}

inline float msToFpm(float ms)
{
    return ms * 196.8504f;  // m/s -> ft/min
}

} // namespace dcs_units

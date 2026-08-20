#pragma once

#include <cstring>

// Unit conversion for the DCS display. DCS sends raw metric values (meters,
// m/s); conversion to the display unit system happens here on the ESP32.
namespace dcs_units
{

// Aircraft using the metric system (prefix match).
inline constexpr const char *kMetricAircraft[] = {
    "Su-25", "Su-27", "Su-33",
    "MiG-15", "MiG-19", "MiG-21", "MiG-23", "MiG-29",
    "Ka-50", "Ka-50_3", "Mi-8", "Mi-24",
    "Yak-52", "L-39", "J-11",
};

inline bool aircraftUsesMetric(const char *name)
{
    for (const char *p : kMetricAircraft)
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
    return aircraftUsesMetric(name);
}

inline float metersToFeet(float meters)
{
    return meters * 3.28084f;
}

inline float metersPerSecondToKnots(float metersPerSecond)
{
    return metersPerSecond * 1.943844f;
}

inline float metersPerSecondToKilometersPerHour(float metersPerSecond)
{
    return metersPerSecond * 3.6f;
}

inline float metersPerSecondToFeetPerMinute(float metersPerSecond)
{
    return metersPerSecond * 196.8504f;  // m/s -> ft/min
}

} // namespace dcs_units

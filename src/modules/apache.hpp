#pragma once

#include <cstring>

// Apache (AH-64D) unit conversion rules.
namespace dcs_units
{
// Apache keeps metric altitude but uses imperial speed.
inline bool apacheSpeedImperial(const char *name)
{
    return strncmp(name, "AH-64", 5) == 0;
}
} // namespace dcs_units

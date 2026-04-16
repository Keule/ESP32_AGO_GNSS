/**
 * @file gnss.cpp
 * @brief GNSS NMEA parser and state update logic.
 */

#include "gnss.h"

#include "dependency_policy.h"
#include "features.h"
#include "hal/hal.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace gnss {

#if FEAT_GNSS

static const char* nmeaField(const char* sentence, int field_index) {
    const char* p = sentence;
    for (int i = 0; i < field_index; i++) {
        p = std::strchr(p, ',');
        if (!p) return nullptr;
        p++;
    }
    return p;
}

static double nmeaDouble(const char* field) {
    if (!field || *field == ',' || *field == '\0' || *field == '*') return 0.0;
    char* end = nullptr;
    return strtod(field, &end);
}

static int nmeaInt(const char* field) {
    if (!field || *field == ',' || *field == '\0' || *field == '*') return 0;
    return static_cast<int>(strtol(field, nullptr, 10));
}

static bool nmeaStartsWith(const char* sentence, const char* prefix) {
    return std::strncmp(sentence, prefix, std::strlen(prefix)) == 0;
}

static double nmeaLatToDecimal(double raw, char hemisphere) {
    double deg = std::floor(raw / 100.0);
    double minutes = raw - deg * 100.0;
    double decimal = deg + minutes / 60.0;
    if (hemisphere == 'S' || hemisphere == 's') decimal = -decimal;
    return decimal;
}

static double nmeaLonToDecimal(double raw, char hemisphere) {
    double deg = std::floor(raw / 100.0);
    double minutes = raw - deg * 100.0;
    double decimal = deg + minutes / 60.0;
    if (hemisphere == 'W' || hemisphere == 'w') decimal = -decimal;
    return decimal;
}

bool nmeaParseGGA(const char* sentence, NavigationState* state) {
    if (!nmeaStartsWith(sentence, "$GN") && !nmeaStartsWith(sentence, "$GP")) return false;
    if (!std::strstr(sentence, "GGA")) return false;

    const int fix = nmeaInt(nmeaField(sentence, 6));
    state->gnss_fix_quality = static_cast<uint8_t>(fix);
    if (fix == 0) {
        return false;
    }

    const double lat_raw = nmeaDouble(nmeaField(sentence, 2));
    const char ns = nmeaField(sentence, 3) ? nmeaField(sentence, 3)[0] : 'N';
    const double lon_raw = nmeaDouble(nmeaField(sentence, 4));
    const char ew = nmeaField(sentence, 5) ? nmeaField(sentence, 5)[0] : 'E';
    const float alt_m = static_cast<float>(nmeaDouble(nmeaField(sentence, 9)));
    const int sats = nmeaInt(nmeaField(sentence, 7));
    const float hdop = static_cast<float>(nmeaDouble(nmeaField(sentence, 8)));

    state->gnss_lat_deg = nmeaLatToDecimal(lat_raw, ns);
    state->gnss_lon_deg = nmeaLonToDecimal(lon_raw, ew);
    state->gnss_alt_m = alt_m;
    state->gnss_num_sats = static_cast<uint8_t>(sats < 0 ? 0 : sats);
    state->gnss_hdop = hdop;
    return true;
}

bool nmeaParseRMC(const char* sentence, NavigationState* state) {
    if (!nmeaStartsWith(sentence, "$GN") && !nmeaStartsWith(sentence, "$GP")) return false;
    if (!std::strstr(sentence, "RMC")) return false;

    const char* statusField = nmeaField(sentence, 2);
    if (!statusField || statusField[0] != 'A') return false;

    const double sog_knots = nmeaDouble(nmeaField(sentence, 7));
    const float cog_deg = static_cast<float>(nmeaDouble(nmeaField(sentence, 8)));

    state->gnss_sog_mps = static_cast<float>(sog_knots * 0.514444);
    state->gnss_cog_deg = cog_deg;
    return true;
}

void init() {
    hal_gnss_init();
    hal_log("GNSS: parser enabled");
}

void pollMain() {
    char line[256];
    while (hal_gnss_main_read_line(line, sizeof(line))) {
        const uint32_t now = hal_millis();
        StateLock lock;

        bool gga_ok = false;
        bool rmc_ok = false;
        if (std::strstr(line, "GGA")) {
            gga_ok = nmeaParseGGA(line, &g_nav);
            if (gga_ok) g_nav.gnss_fix_timestamp_ms = now;
        }
        if (std::strstr(line, "RMC")) {
            rmc_ok = nmeaParseRMC(line, &g_nav);
            if (rmc_ok) g_nav.gnss_motion_timestamp_ms = now;
        }

        if (gga_ok || rmc_ok) {
            g_nav.timestamp_ms = now;
            g_nav.gnss_quality_ok =
                dep_policy::isGnssFixValid(now, g_nav.gnss_fix_timestamp_ms,
                                           g_nav.gnss_fix_quality, g_nav.gnss_hdop);
        }
    }
}

void pollHeading() {
    char line[256];
    while (hal_gnss_heading_read_line(line, sizeof(line))) {
        if (!std::strstr(line, "RMC")) continue;

        const uint32_t now = hal_millis();
        StateLock lock;
        if (nmeaParseRMC(line, &g_nav)) {
            g_nav.heading_deg = g_nav.gnss_cog_deg;
            g_nav.heading_timestamp_ms = now;
            g_nav.heading_quality_ok = dep_policy::isHeadingPlausible(g_nav.heading_deg);
            g_nav.timestamp_ms = now;
        }
    }
}

#else

bool nmeaParseGGA(const char*, NavigationState*) { return false; }
bool nmeaParseRMC(const char*, NavigationState*) { return false; }
void init() {}
void pollMain() {}
void pollHeading() {}

#endif

}  // namespace gnss

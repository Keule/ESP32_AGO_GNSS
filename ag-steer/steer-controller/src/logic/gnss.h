/**
 * @file gnss.h
 * @brief GNSS module – NMEA parser for GGA and RMC sentences.
 */

#pragma once

#include "global_state.h"

namespace gnss {

void init();
void pollMain();
void pollHeading();

bool nmeaParseGGA(const char* sentence, NavigationState* state);
bool nmeaParseRMC(const char* sentence, NavigationState* state);

}  // namespace gnss

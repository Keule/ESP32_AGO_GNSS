#pragma once

/**
 * @file features.h
 * @brief Zentrale Feature-Flags und Helper fuer Build-Profile.
 *
 * Alle Feature-Entscheidungen im Code sollen ueber diesen Header laufen.
 */

// -------------------------------------------------------------------
// Roh-Flags aus Build-System (platformio.ini -D...) auf 0/1 normieren.
// Wichtig: Build-Defines selbst NICHT neu definieren.
// -------------------------------------------------------------------
#if defined(FEAT_PROFILE_COMM_ONLY)
#define FEAT_CFG_PROFILE_COMM_ONLY 1
#else
#define FEAT_CFG_PROFILE_COMM_ONLY 0
#endif

#if defined(FEAT_PROFILE_SENSOR_FRONT)
#define FEAT_CFG_PROFILE_SENSOR_FRONT 1
#else
#define FEAT_CFG_PROFILE_SENSOR_FRONT 0
#endif

#if defined(FEAT_PROFILE_ACTOR_REAR)
#define FEAT_CFG_PROFILE_ACTOR_REAR 1
#else
#define FEAT_CFG_PROFILE_ACTOR_REAR 0
#endif

#if defined(FEAT_PROFILE_FULL_STEER)
#define FEAT_CFG_PROFILE_FULL_STEER 1
#else
#define FEAT_CFG_PROFILE_FULL_STEER 0
#endif

#define FEAT_CFG_PROFILE_ANY ( \
    FEAT_CFG_PROFILE_COMM_ONLY || \
    FEAT_CFG_PROFILE_SENSOR_FRONT || \
    FEAT_CFG_PROFILE_ACTOR_REAR || \
    FEAT_CFG_PROFILE_FULL_STEER)

// Feature-Rohwerte aus expliziten -D FEAT_... Flags
#if defined(FEAT_COMM_ETH)
#define FEAT_CFG_RAW_COMM_ETH 1
#else
#define FEAT_CFG_RAW_COMM_ETH 0
#endif

#if defined(FEAT_SENSOR_FRONT)
#define FEAT_CFG_RAW_SENSOR_FRONT 1
#else
#define FEAT_CFG_RAW_SENSOR_FRONT 0
#endif

#if defined(FEAT_IMU_FRONT)
#define FEAT_CFG_RAW_IMU_FRONT 1
#else
#define FEAT_CFG_RAW_IMU_FRONT 0
#endif

#if defined(FEAT_ACTOR_REAR)
#define FEAT_CFG_RAW_ACTOR_REAR 1
#else
#define FEAT_CFG_RAW_ACTOR_REAR 0
#endif

#if defined(FEAT_CONTROL_LOOP)
#define FEAT_CFG_RAW_CONTROL_LOOP 1
#else
#define FEAT_CFG_RAW_CONTROL_LOOP 0
#endif

#if defined(FEAT_PID_STEER)
#define FEAT_CFG_RAW_PID_STEER 1
#else
#define FEAT_CFG_RAW_PID_STEER 0
#endif

// Profil-Defaults (falls Profil aktiv, aber einzelne Raw-Flags fehlen)
#define FEAT_CFG_PROF_COMM \
    (FEAT_CFG_PROFILE_COMM_ONLY || FEAT_CFG_PROFILE_SENSOR_FRONT || \
     FEAT_CFG_PROFILE_ACTOR_REAR || FEAT_CFG_PROFILE_FULL_STEER)
#define FEAT_CFG_PROF_SENSOR (FEAT_CFG_PROFILE_SENSOR_FRONT || FEAT_CFG_PROFILE_FULL_STEER)
#define FEAT_CFG_PROF_IMU    (FEAT_CFG_PROFILE_SENSOR_FRONT || FEAT_CFG_PROFILE_FULL_STEER)
#define FEAT_CFG_PROF_ACTOR  (FEAT_CFG_PROFILE_ACTOR_REAR || FEAT_CFG_PROFILE_FULL_STEER)
#define FEAT_CFG_PROF_CONTROL (FEAT_CFG_PROFILE_ACTOR_REAR || FEAT_CFG_PROFILE_FULL_STEER)
#define FEAT_CFG_PROF_PID    (FEAT_CFG_PROFILE_FULL_STEER)

// Legacy-Default: ohne Profil alles aktiv (bisheriges Verhalten).
#define FEAT_CFG_DEFAULT_ON (!FEAT_CFG_PROFILE_ANY)

// -------------------------------------------------------------------
// Abgeleitete, zentrale Feature-Makros
// -------------------------------------------------------------------
#define FEAT_COMM_ETH_NORM     (FEAT_CFG_RAW_COMM_ETH || (FEAT_CFG_PROF_COMM && !FEAT_CFG_RAW_COMM_ETH) || FEAT_CFG_DEFAULT_ON)
#define FEAT_SENSOR_FRONT_NORM (FEAT_CFG_RAW_SENSOR_FRONT || (FEAT_CFG_PROF_SENSOR && !FEAT_CFG_RAW_SENSOR_FRONT) || FEAT_CFG_DEFAULT_ON)
#define FEAT_IMU_FRONT_NORM    (FEAT_CFG_RAW_IMU_FRONT || (FEAT_CFG_PROF_IMU && !FEAT_CFG_RAW_IMU_FRONT) || FEAT_CFG_DEFAULT_ON)
#define FEAT_ACTOR_REAR_NORM   (FEAT_CFG_RAW_ACTOR_REAR || (FEAT_CFG_PROF_ACTOR && !FEAT_CFG_RAW_ACTOR_REAR) || FEAT_CFG_DEFAULT_ON)
#define FEAT_CONTROL_LOOP_NORM (FEAT_CFG_RAW_CONTROL_LOOP || (FEAT_CFG_PROF_CONTROL && !FEAT_CFG_RAW_CONTROL_LOOP) || FEAT_CFG_DEFAULT_ON)
#define FEAT_PID_STEER_NORM    (FEAT_CFG_RAW_PID_STEER || (FEAT_CFG_PROF_PID && !FEAT_CFG_RAW_PID_STEER) || FEAT_CFG_DEFAULT_ON)

#define FEAT_COMM      (FEAT_COMM_ETH_NORM)
#define FEAT_SENSOR    (FEAT_SENSOR_FRONT_NORM)
#define FEAT_IMU       (FEAT_SENSOR && FEAT_IMU_FRONT_NORM)
#define FEAT_ACTOR     (FEAT_ACTOR_REAR_NORM)
#define FEAT_CONTROL   (FEAT_CONTROL_LOOP_NORM && FEAT_SENSOR && FEAT_ACTOR)
#define FEAT_PID       (FEAT_CONTROL && FEAT_PID_STEER_NORM)
#define FEAT_STEER_ALL (FEAT_COMM && FEAT_SENSOR && FEAT_ACTOR && FEAT_CONTROL)

static_assert(FEAT_COMM, "FEAT_COMM muss aktiv sein (mindestens Ethernet/UDP Kommunikation).");

// -------------------------------------------------------------------
// Helper-Makros/Funktionen fuer Feature-Abfragen
// -------------------------------------------------------------------
#define FEAT_ENABLED(flag_macro) ((flag_macro) != 0)
#define FEAT_DISABLED(flag_macro) ((flag_macro) == 0)

namespace feat {
inline constexpr bool comm()    { return FEAT_ENABLED(FEAT_COMM); }
inline constexpr bool sensor()  { return FEAT_ENABLED(FEAT_SENSOR); }
inline constexpr bool imu()     { return FEAT_ENABLED(FEAT_IMU); }
inline constexpr bool actor()   { return FEAT_ENABLED(FEAT_ACTOR); }
inline constexpr bool control() { return FEAT_ENABLED(FEAT_CONTROL); }
inline constexpr bool pid()     { return FEAT_ENABLED(FEAT_PID); }
}  // namespace feat

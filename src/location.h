#ifndef LOCATION_H
#define LOCATION_H

#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include <nrf_modem_at.h>
#include <modem/lte_lc.h>
#include <modem/location.h>
#include <modem/nrf_modem_lib.h>
#include <date_time.h>
#include <net/nrf_cloud_pgps.h>

#include <dk_buttons_and_leds.h> // For DK board LEDs/buttons

extern struct k_sem location_start_thread;

/**
 * @brief Initialize the location service system.
 *
 * @details Performs the following initialization steps:
 * 1. Initializes LED library
 * 2. Initializes modem library
 * 3. Registers LTE event handler
 * 4. Enables Power Saving Mode (PSM)
 * 5. Connects to LTE network
 * 6. Initializes location service
 * 7. Initializes request mutex
 *
 * @return 0 on success, negative error code on failure
 */
int location_init_system(void);

/**
 * @brief Retrieve location with default configuration.
 *
 * @details Uses both GNSS and cellular methods with custom timeout settings.
 *          GNSS timeout: 120 seconds
 *          Cellular timeout: 40 seconds
 *
 * @note This function is thread-safe and uses a mutex to protect location data.
 *       It will block until location is retrieved or an error occurs.
 */
void location_default_get(void);

/**
 * @brief Starts the location Finite State Machine (FSM).
 *
 * This function should be run in a dedicated thread and will handle:
 * - Initialization of location services
 * - Periodic location retrieval
 * - Error handling and recovery
 */
void locationFSM(void);

#endif /* LOCATION_H */
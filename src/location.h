#ifndef LOCATION_H
#define LOCATION_H

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

#include <nrf_modem_gnss.h> // Import for GPS
#include <modem/lte_lc.h>

#include <dk_buttons_and_leds.h> // For DK board LEDs/buttons

/* GNSS fix result buffer size */
#define GPS_MESSAGE_SIZE 256

/**
 * @brief Initialize the GNSS subsystem
 * @return 0 on success, negative error code on failure
 */
int gnss_init(void);

/**
 * @brief Request a single GNSS location fix
 * @return 0 if fix request started successfully, negative error code on failure
 */
int request_location(void);

/**
 * @brief Get last valid GNSS data
 * @param buf Buffer to store formatted location string
 * @param len Length of buffer
 * @return 0 if valid data exists, negative otherwise
 */
int get_gnss_data(char *buf, size_t len);

void locationFSM(void);

#endif /* LOCATION_H */
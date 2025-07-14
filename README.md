# Location Tracker for nRF9151 DK

## Overview
This firmware runs on Nordic Semiconductor's nRF9160 DK, implementing a Finite State Machine (FSM) that periodically obtains the device's location using GNSS positioning through LTE cellular connectivity. The solution is ideal for asset tracking, fleet management, and location-based IoT applications.

## Key Features
- **State Machine Architecture** with four operational states:
  - `INIT`: Initializes modem and location services
  - `IDLE`: Waits between location requests
  - `GET_DATA`: Retrieves current location via GNSS
  - `ERROR`: Handles initialization failures
- **Configurable update intervals** (default: 30 minutes)
- **Google Maps URL generation** for position visualization
- **LTE Power Saving Mode (PSM)** for energy efficiency
- **Robust error handling** with automatic retries
- **LED status indicators** for visual feedback

## Hardware Requirements
1. [nRF9151 DK](https://www.nordicsemi.com/Products/Development-hardware/nRF9151-DK) development board
2. Active LTE-M/NB-IoT SIM card
3. External GNSS antenna (included with DK)
4. USB-C cable for power/programming

## Software Dependencies
- nRF Connect SDK v2.0+
- Zephyr RTOS v3.4+
- Required Zephyr modules:
  - Modem library
  - LTE Link Control
  - Location library
  - Date Time library

## Resources
- Cellular Location Sample
   - https://github.com/nrfconnect/sdk-nrf/blob/main//samples/cellular/location
- New Location Library
   - https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/libraries/modem/location.html#lib-location

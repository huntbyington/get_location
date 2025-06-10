#include "location.h"

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

#include <dk_buttons_and_leds.h> // For DK board LEDs/buttons

#include <modem/lte_lc.h>        // LTE link control (required for cellular)
#include <modem/nrf_modem_lib.h> // Modem initialization
#include <modem/modem_info.h>    // Signal strength, network info
#include <zephyr/net/net_mgmt.h> // Network management (for connection events)

LOG_MODULE_REGISTER(GET_LOCATION, LOG_LEVEL_INF);

K_SEM_DEFINE(location_start_thread, 0, 1);
K_THREAD_DEFINE(location_fsm_tid, CONFIG_LOCATION_THREAD_STACKSIZE, locationFSM,
                NULL, NULL, NULL, CONFIG_LOCATION_THREAD_PRIORITY, 0, 0);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

static K_SEM_DEFINE(run_sample, 0, 1);

static void lte_handler(const struct lte_lc_evt *const evt)
{
    switch (evt->type)
    {
    /* On changed registration status, print status */
    case LTE_LC_EVT_NW_REG_STATUS:
        if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
            (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING))
        {
            break;
        }
        LOG_INF("Network registration status: %s",
                evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "Connected - home network" : "Connected - roaming");
        k_sem_give(&run_sample);

        dk_set_led_on(DK_LED2);

        break;
    /* On event RRC update, print RRC mode */
    case LTE_LC_EVT_RRC_UPDATE:
        LOG_INF("RRC mode: %s", evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
        break;
    // case LTE_LC_EVT_PSM_UPDATE:
    //     LOG_INF("PSM parameter update: TAU: %d, Active time: %d",
    //             evt->psm_cfg.tau, evt->psm_cfg.active_time);
    //     if (evt->psm_cfg.active_time == -1)
    //     {
    //         LOG_ERR("Network rejected PSM parameters. Failed to enable PSM");
    //     }
    //     break;
    default:
        break;
    }
}

/* modem_configure() initializes an LTE connection */
static int modem_configure(void)
{
    int err;

    LOG_INF("Initializing modem library");
    err = nrf_modem_lib_init();
    if (err)
    {
        LOG_ERR("Failed to initialize the modem library, error: %d", err);
        return err;
    }

    LOG_INF("Connecting to LTE network");
    err = lte_lc_connect_async(lte_handler);
    if (err)
    {
        LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
        return err;
    }
    return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
    int err;

    switch (has_changed)
    {
    case DK_BTN1_MSK:
        if (button_state & DK_BTN1_MSK)
        {
            err = request_location();
            if (err)
            {
                LOG_ERR("Location request failed");
            }
        }
        break;
    case DK_BTN2_MSK:
        if (button_state & DK_BTN2_MSK)
        {
            char location_buffer[128];
            err = get_location_data(location_buffer, sizeof(location_buffer));
            if (err < 0)
            {
                LOG_ERR("Location request failed");
            }
            else
            {
                LOG_INF("Location: %s", location_buffer);
            }
        }
        break;
    }
}

int main(void)
{
    int err = 0;

    err = dk_leds_init();
    if (err != 0)
    {
        LOG_ERR("Failed to initialize the LED library");
    }

    /* Initializes lte modem */
    err = modem_configure();
    if (err)
    {
        LOG_ERR("Failed to configure the modem");
        return 0;
    }

    // err = gnss_init();
    // if (err)
    // {
    //     LOG_ERR("Failed to initialize GNSS");
    //     return 0;
    // }

    if (dk_buttons_init(button_handler) != 0)
    {
        LOG_ERR("Failed to initialize the buttons library\n");
    }

    k_sem_take(&run_sample, K_FOREVER);

    k_sem_give(&location_start_thread);
}
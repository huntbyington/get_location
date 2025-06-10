#include "location.h"

LOG_MODULE_REGISTER(LOCATION, LOG_LEVEL_DBG);

extern struct k_sem location_start_thread;

static struct k_mutex request_mutex; // Protects location data variables, no two processes should read and write at the same time

enum location_state
{
    STATE_INIT,
    STATE_IDLE,
    STATE_GET_DATA,
    STATE_ERROR
};
static enum location_state locationState = STATE_INIT;

struct k_sem data_retrieved_sem;

// static struct k_work_delayable rx_work;
static struct nrf_modem_gnss_pvt_data_frame pvt_data;

static int64_t gnss_start_time;
static bool first_fix = false;

static bool gnss_running = false;

static void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
    LOG_INF("Latitude:       %d", (int)pvt_data->latitude);
    LOG_INF("Longitude:      %d", (int)pvt_data->longitude);
    LOG_INF("Altitude:       %d m", (int)pvt_data->altitude);
    LOG_INF("Time (UTC):     %02u:%02u:%02u.%03u",
            pvt_data->datetime.hour,
            pvt_data->datetime.minute,
            pvt_data->datetime.seconds,
            pvt_data->datetime.ms);
}

static void gnss_event_handler(int event)
{
    int err;

    switch (event)
    {
    case NRF_MODEM_GNSS_EVT_PVT:
        LOG_INF("Searching...");
        int num_satellites = 0;
        for (int i = 0; i < 12; i++)
        {
            if (pvt_data.sv[i].signal != 0)
            {
                LOG_INF("sv: %d, cn0: %d, signal: %d", pvt_data.sv[i].sv, pvt_data.sv[i].cn0, pvt_data.sv[i].signal);
                num_satellites++;
            }
        }
        LOG_INF("Number of current satellites: %d", num_satellites);
        err = nrf_modem_gnss_read(&pvt_data, sizeof(pvt_data), NRF_MODEM_GNSS_DATA_PVT);
        if (err)
        {
            LOG_ERR("nrf_modem_gnss_read failed, err %d", err);
            nrf_modem_gnss_stop();
            gnss_running = false;
            return;
        }
        if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID)
        {
            dk_set_led_on(DK_LED1);
            print_fix_data(&pvt_data);
            if (!first_fix)
            {
                LOG_INF("Time to first fix: %2.1lld s", (k_uptime_get() - gnss_start_time) / 1000);
                first_fix = true;
            }
            LOG_DBG("Line 69");
            // nrf_modem_gnss_stop();
            gnss_running = false;
            k_sem_give(&data_retrieved_sem);
            return;
        }
        break;

    case NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:
        LOG_INF("GNSS has woken up");
        break;

    case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:
        LOG_INF("GNSS enter sleep after fix");
        break;

    default:
        break;
    }
}

int gnss_init(void)
{

    if (dk_leds_init() != 0)
    {
        LOG_ERR("Failed to initialize the LED library");
    }

    // Activates GNSS
    if (lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS) != 0)
    {
        LOG_ERR("Failed to activate GNSS functional mode");
        return 0;
    }

    // Registers GNSS event handler
    if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0)
    {
        LOG_ERR("Failed to set GNSS event handler");
        return -1;
    }

    // Sets retry period for when GNSS doen't get a lock
    if (nrf_modem_gnss_fix_interval_set(CONFIG_GNSS_PERIODIC_INTERVAL) != 0)
    {
        LOG_ERR("Failed to set GNSS fix interval");
        return -1;
    }

    if (nrf_modem_gnss_fix_retry_set(CONFIG_GNSS_PERIODIC_TIMEOUT) != 0)
    {
        LOG_ERR("Failed to set GNSS fix retry");
        return -1;
    }

    k_mutex_init(&request_mutex);

    k_sem_init(&data_retrieved_sem, 0, 1);

    return 0;
}

int request_location(void)
{
    if (k_mutex_lock(&request_mutex, K_MSEC(100)))
    {
        LOG_ERR("Failed to lock request location function");
        return -1;
    }

    if (gnss_running)
    {
        LOG_WRN("GNSS already running");
        nrf_modem_gnss_stop();
    }

    LOG_INF("Starting GNSS fix request");
    gnss_start_time = k_uptime_get();

    if (nrf_modem_gnss_start() != 0)
    {
        LOG_ERR("Failed to start GNSS");
        return -1;
    }

    gnss_running = true;

    k_mutex_unlock(&request_mutex);
    return 0;
}

int get_gnss_data(char *buf, size_t len)
{
    if (k_mutex_lock(&request_mutex, K_MSEC(100)))
    {
        LOG_ERR("Failed to lock request location function");
        return -1;
    }

    if (!first_fix)
    {
        LOG_DBG("No first fix");
        return -ENODATA;
    }

    k_mutex_unlock(&request_mutex);
    return snprintf(buf, len, "Lat: %d, Lon: %d",
                    (int)pvt_data.latitude, (int)pvt_data.longitude);
}

void locationFSM(void)
{
    k_sem_take(&location_start_thread, K_FOREVER);
    LOG_INF("Starting Location thread");
    while (true)
    {
        switch (locationState)
        {
        case STATE_INIT:
            if (gnss_init())
            {
                locationState = STATE_ERROR;
                break;
            }
            LOG_DBG("INIT -> IDLE");
            locationState = STATE_IDLE;
            break;
        case STATE_IDLE:
            k_msleep(CONFIG_GET_LOCATION_INTERVAL * 60000);
            LOG_DBG("IDLE -> GET");
            locationState = STATE_GET_DATA;
            break;
        case STATE_GET_DATA:
            request_location();
            LOG_DBG("GET -> IDLE");
            locationState = STATE_IDLE;
            k_sem_take(&data_retrieved_sem, K_FOREVER);
            break;
        case STATE_ERROR:
            LOG_ERR("Button initialization error retrying in 5s");
            k_msleep(5000);
            locationState = STATE_INIT;
            break;
        }
    }
}
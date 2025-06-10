#include "location.h"

LOG_MODULE_REGISTER(LOCATION, LOG_LEVEL_DBG);

extern struct k_sem location_start_thread;

static struct k_mutex request_mutex; // Protects location data variables, no two processes should read and write at the same time
struct k_sem data_retrieved_sem;

enum location_state
{
    STATE_INIT,
    STATE_IDLE,
    STATE_GET_DATA,
    STATE_ERROR
};
static enum location_state locationState = STATE_INIT;

static struct location_data location_data;
static bool location_retrieved = false;

// // static struct k_work_delayable rx_work;
// static struct nrf_modem_gnss_pvt_data_frame pvt_data;

// static int64_t gnss_start_time;
// static bool first_fix = false;

// static bool gnss_running = false;

static void
print_fix_data(void)
{
    // Print location information
    LOG_INF("Latitude: %.6f", location_data.latitude / 1000000.0);
    LOG_INF("Longitude: %.6f", location_data.longitude / 1000000.0);
    LOG_INF("Accuracy: %.1f m", (double)location_data.accuracy);
}

static void location_event_handler(const struct location_event_data *event_data)
{
    switch (event_data->id)
    {
    case LOCATION_EVT_LOCATION:
        LOG_INF("Location retrieved successfully");

        // Copy location data
        memcpy(&location_data, &event_data->location, sizeof(struct location_data));
        location_retrieved = true;

        print_fix_data();

        // if (location_data.method == LOCATION_METHOD_GNSS)
        // {
        //     dk_set_led_on(DK_LED1);
        // }
        // else if (location_data.method == LOCATION_METHOD_CELLULAR)
        // {
        //     dk_set_led_on(DK_LED2); // Use a different LED for cellular
        // }

        k_sem_give(&data_retrieved_sem);
        break;

    case LOCATION_EVT_TIMEOUT:
        LOG_WRN("Location request timed out");
        k_sem_give(&data_retrieved_sem);
        break;

    case LOCATION_EVT_ERROR:
        LOG_ERR("Location request failed");
        k_sem_give(&data_retrieved_sem);
        break;

    case LOCATION_EVT_FALLBACK:
        LOG_INF("Falling triggered");
        break;

    default:
        break;
    }
}

int location_init_system(void)
{
    int err;

    if (dk_leds_init() != 0)
    {
        LOG_ERR("Failed to initialize the LED library");
        return -1;
    }

    // Initialize the Location library
    err = location_init(location_event_handler);
    if (err)
    {
        LOG_ERR("Failed to initialize location library, error: %d", err);
        return -1;
    }

    // Register the event handler
    err = location_handler_register(location_event_handler);
    if (err)
    {
        LOG_ERR("Failed to register location handler, error: %d", err);
        return -1;
    }

    k_mutex_init(&request_mutex);
    k_sem_init(&data_retrieved_sem, 0, 1);

    return 0;
}

int request_location(void)
{
    int err;

    if (k_mutex_lock(&request_mutex, K_MSEC(100)))
    {
        LOG_ERR("Failed to lock request location function");
        return -1;
    }

    // Configure location request with fallback
    struct location_config config;
    enum location_method methods[] = {LOCATION_METHOD_GNSS, LOCATION_METHOD_CELLULAR};

    // Set default values for the methods
    location_config_defaults_set(&config, ARRAY_SIZE(methods), methods);

    // Configure request mode as fallback
    config.mode = LOCATION_REQ_MODE_FALLBACK;

    // Set timeouts
    config.timeout = 180 * MSEC_PER_SEC; // Overall timeout
    config.methods[0].gnss.timeout = CONFIG_GNSS_PERIODIC_TIMEOUT * MSEC_PER_SEC;
    config.methods[1].cellular.timeout = 15 * MSEC_PER_SEC;

    // Request location
    LOG_INF("Starting location request with GNSS and cellular fallback");
    err = location_request(&config);
    if (err)
    {
        LOG_ERR("Failed to request location, error: %d", err);
        k_mutex_unlock(&request_mutex);
        return -1;
    }

    k_mutex_unlock(&request_mutex);
    return 0;
}

int get_location_data(char *buf, size_t len)
{
    if (k_mutex_lock(&request_mutex, K_MSEC(100)))
    {
        LOG_ERR("Failed to lock request location function");
        return -1;
    }

    if (!location_retrieved)
    {
        LOG_DBG("No location data available");
        k_mutex_unlock(&request_mutex);
        return -ENODATA;
    }

    int ret = snprintf(buf, len, "Lat: %.6f, Lon: %.6f, Accuracy: %.1f m",
                       location_data.latitude / 1000000.0,
                       location_data.longitude / 1000000.0,
                       (double)location_data.accuracy);

    k_mutex_unlock(&request_mutex);
    return ret;
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
            if (location_init_system())
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
            LOG_ERR("Location initialization error retrying in 5s");
            k_msleep(5000);
            locationState = STATE_INIT;
            break;
        }
    }
}
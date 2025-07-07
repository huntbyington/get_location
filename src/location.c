#include "location.h"

LOG_MODULE_REGISTER(LOCATION, LOG_LEVEL_DBG);

static struct k_mutex request_mutex; // Protects location data variables, no two processes should read and write at the same time

enum location_state
{
    STATE_INIT,
    STATE_IDLE,
    STATE_GET_DATA,
    STATE_ERROR
};
static enum location_state locationState = STATE_INIT;

static struct location_event_data location_data;

static K_SEM_DEFINE(location_event, 0, 1);

static K_SEM_DEFINE(time_update_finished, 0, 1);

static void date_time_evt_handler(const struct date_time_evt *evt)
{
    k_sem_give(&time_update_finished);
}

static void print_fix_data(void)
{
    // Print location information
    LOG_INF("Printing location:\n");
    LOG_INF("  method: %s\n", location_method_str(location_data.method));
    LOG_INF("  Latitude: %.6f", location_data.location.latitude);
    LOG_INF("  Longitude: %.6f", location_data.location.longitude);
    LOG_INF("  Accuracy: %.1f m", (double)location_data.location.accuracy);
    LOG_INF("  Google maps URL: https://maps.google.com/?q=%.06f,%.06f\n\n",
            location_data.location.latitude, location_data.location.longitude);
}

static void location_event_handler(const struct location_event_data *event_data)
{
    switch (event_data->id)
    {
    case LOCATION_EVT_LOCATION:
        location_data = *event_data;
        print_fix_data();
        break;

    case LOCATION_EVT_TIMEOUT:
        LOG_INF("Getting location timed out\n\n");
        break;

    case LOCATION_EVT_ERROR:
        LOG_INF("Getting location failed\n\n");
        break;

    case LOCATION_EVT_GNSS_ASSISTANCE_REQUEST:
        LOG_INF("Getting location assistance requested (A-GNSS). Not doing anything.\n\n");
        break;

    case LOCATION_EVT_GNSS_PREDICTION_REQUEST:
        LOG_INF("Getting location assistance requested (P-GPS). Not doing anything.\n\n");
        break;

    default:
        LOG_INF("Getting location: Unknown event\n\n");
        break;
    }

    k_sem_give(&location_event);
}

static void location_event_wait(void)
{
    k_sem_take(&location_event, K_FOREVER);
}

int location_init_system(void)
{
    int err;

    if (dk_leds_init() != 0)
    {
        LOG_ERR("Failed to initialize the LED library");
        return -1;
    }

    /* Enable PSM. */
    lte_lc_psm_req(true);
    lte_lc_connect();

    err = location_init(location_event_handler);
    if (err)
    {
        LOG_INF("Initializing the Location library failed, error: %d\n", err);
        return -1;
    }

    k_mutex_init(&request_mutex);
    // k_sem_init(&data_retrieved_sem, 0, 1);

    return 0;
}

static void location_init_get(void)
{
    k_mutex_lock(&request_mutex, K_FOREVER);

    int err;
    struct location_config config;
    enum location_method methods[] = {LOCATION_METHOD_GNSS};

    location_config_defaults_set(&config, ARRAY_SIZE(methods), methods);
    /* GNSS timeout is set to ten milliseconds, function is meant to update PGPS data upon init. */
    config.methods[0].gnss.timeout = 10;

    err = location_request(&config);
    if (err)
    {
        LOG_INF("Requesting location failed, error: %d\n", err);
        return;
    }

    location_event_wait();

    k_mutex_unlock(&request_mutex);
}

void location_default_get(void)
{
    k_mutex_lock(&request_mutex, K_FOREVER);

    int err;
    struct location_config config;
    enum location_method methods[] = {LOCATION_METHOD_GNSS};

    location_config_defaults_set(&config, ARRAY_SIZE(methods), methods);
    /* GNSS timeout is set to forever. */
    config.methods[0].gnss.timeout = SYS_FOREVER_MS;

    LOG_INF("Requesting location with the default configuration...\n");

    err = location_request(&config);
    if (err)
    {
        LOG_INF("Requesting location failed, error: %d\n", err);
        return;
    }

    location_event_wait();

    k_mutex_unlock(&request_mutex);
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
            location_init_get();
            LOG_DBG("INIT -> IDLE");
            locationState = STATE_IDLE;
            break;
        case STATE_IDLE:
            k_msleep(CONFIG_GET_LOCATION_INTERVAL * 60000);
            LOG_DBG("IDLE -> GET");
            locationState = STATE_GET_DATA;
            break;
        case STATE_GET_DATA:
            location_default_get();
            LOG_DBG("GET -> IDLE");
            locationState = STATE_IDLE;
            break;
        case STATE_ERROR:
            LOG_ERR("Location initialization error retrying in 5s");
            k_msleep(5000);
            locationState = STATE_INIT;
            break;
        }
    }
}
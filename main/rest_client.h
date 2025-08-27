esp_err_t rc_login(const char *email, const char *password, bool use_cache);
esp_err_t rc_extract_device_config(const char *device_json,
                                   int *readingsPerBatch,
                                   int *wakeUpInterval);
esp_err_t rc_post_readings(const char *json_payload);
esp_err_t rc_get_device_by_mac(const char *mac, char **out_json);
bool rc_load_creds(char *email, size_t e_len, char *pass, size_t p_len);
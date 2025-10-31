esp_err_t rc_login(const char *email, const char *password, bool use_cache);
esp_err_t rc_extract_device_config(const char *device_json,
                                   int *readingsPerBatch,
                                   int *wakeUpInterval);
esp_err_t rc_post_readings(const char *json_payload);
esp_err_t rc_get_device_by_mac(const char *mac, char **out_json);
bool rc_load_creds(char *email, size_t e_len, char *pass, size_t p_len);
esp_err_t rc_get_meshes(char **out_json);
esp_err_t rc_update_device(const char *name, int readingsPerBatch, int wakeUpInterval, const char *mesh_id, const char *role);
esp_err_t rc_register_device(const char *name, const char *role, const char *mesh_id, int readingsPerBatch, int wakeUpInterval, int x, int y);
/* Register a new user in the cloud, then perform login to store token on success */
esp_err_t rc_register_user(const char *email, const char *password, const char *name);
esp_err_t rc_create_mesh(const char *name, double lat, double lon);
esp_err_t rc_register_device_for_mac(const char *name,
                                     const char *role,
                                     const char *mesh_id,
                                     const char *mac_address,
                                     int readingsPerBatch,
                                     int wakeUpInterval,
                                     int x, int y);
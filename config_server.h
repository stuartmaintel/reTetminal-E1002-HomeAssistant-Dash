// ============================================================
//  config_server.h
//  Kitchen Panel -- Custom HTTP config API
//
//  Registers REST endpoints on the ESP32 HTTP server that
//  ESPHome's web_server component already starts.
//
//  Endpoints:
//    GET  /api/config            -- returns all config as JSON
//    POST /api/config/wifi       -- {ssid, password}
//    POST /api/config/ha         -- {ip, port, token}
//    POST /api/config/sensors    -- {solar_today, battery_soc, ...}
//    POST /api/config/colors     -- {alarm_inactive:0, door_open:2, ...}
//    POST /api/config/thresholds -- {battery_critical:10, ...}
//    POST /api/config/icons      -- {alarm:"shield-home", ...}
//    POST /api/action/refresh    -- force display refresh
//    POST /api/action/restart    -- reboot device
//
//  All POST bodies are JSON. All responses are JSON.
//  CORS headers included so browser fetch() works from any origin.
// ============================================================

#pragma once
#include "esphome.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/http_request/http_request.h"
#include <esp_http_server.h>
#include <ArduinoJson.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <map>
#include <vector>
#include <algorithm>

// ============================================================
//  MULTI-ENTITY ALERT SLOT HELPERS
// ============================================================

static void slot_entity_key(char* buf, int s, int e) { snprintf(buf, 16, "a_e%d_%d", e, s); }
static void slot_name_key(char* buf, int s, int e)   { snprintf(buf, 16, "a_n%d_%d", e, s); }
static void slot_cond_key(char* buf, int s)    { snprintf(buf, 16, "a_cond_%d",  s); }
static void slot_value_key(char* buf, int s)   { snprintf(buf, 16, "a_value_%d", s); }
static void slot_suffix_key(char* buf, int s)  { snprintf(buf, 16, "a_suffix_%d",s); }
static void slot_enabled_key(char* buf, int s) { snprintf(buf, 16, "a_en_%d",    s); }

// ============================================================
//  poll_alert_states()
//
//  ONE HTTP request per refresh cycle regardless of entity count.
//
//  Strategy: build a Jinja2 template string that renders all
//  configured entity states pipe-separated, POST it to HA's
//  /api/template endpoint, split the response on '|' and map
//  back to slot_state globals.
//
//  Example template sent:
//    {{ states('alarm_control_panel.home') }}|
//    {{ states('binary_sensor.front_door') }}|
//    {{ states('binary_sensor.back_door') }}|...
//
//  Example response:
//    "armed_away|on|off|on|..."
//
//  Deduplication: shared entities appear once in the template,
//  their result is written to all referencing slot globals.
// ============================================================
void poll_alert_states() {
  std::string ha_ip    = nvs_read_str("ha_ip",    "192.168.1.10");
  std::string ha_token = nvs_read_str("ha_token", "");
  int         ha_port  = nvs_read_int("ha_port",   8123);

  if (ha_token.empty()) {
    ESP_LOGW("alerts", "No HA token -- skipping poll");
    return;
  }

  // -- Build slot state pointer matrix -----------------------
  std::string* slot_states[9][8] = {
    { &id(slot_state_1_1), &id(slot_state_1_2), &id(slot_state_1_3), &id(slot_state_1_4),
      &id(slot_state_1_5), &id(slot_state_1_6), &id(slot_state_1_7), &id(slot_state_1_8) },
    { &id(slot_state_2_1), &id(slot_state_2_2), &id(slot_state_2_3), &id(slot_state_2_4),
      &id(slot_state_2_5), &id(slot_state_2_6), &id(slot_state_2_7), &id(slot_state_2_8) },
    { &id(slot_state_3_1), &id(slot_state_3_2), &id(slot_state_3_3), &id(slot_state_3_4),
      &id(slot_state_3_5), &id(slot_state_3_6), &id(slot_state_3_7), &id(slot_state_3_8) },
    { &id(slot_state_4_1), &id(slot_state_4_2), &id(slot_state_4_3), &id(slot_state_4_4),
      &id(slot_state_4_5), &id(slot_state_4_6), &id(slot_state_4_7), &id(slot_state_4_8) },
    { &id(slot_state_5_1), &id(slot_state_5_2), &id(slot_state_5_3), &id(slot_state_5_4),
      &id(slot_state_5_5), &id(slot_state_5_6), &id(slot_state_5_7), &id(slot_state_5_8) },
    { &id(slot_state_6_1), &id(slot_state_6_2), &id(slot_state_6_3), &id(slot_state_6_4),
      &id(slot_state_6_5), &id(slot_state_6_6), &id(slot_state_6_7), &id(slot_state_6_8) },
    { &id(slot_state_7_1), &id(slot_state_7_2), &id(slot_state_7_3), &id(slot_state_7_4),
      &id(slot_state_7_5), &id(slot_state_7_6), &id(slot_state_7_7), &id(slot_state_7_8) },
    { &id(slot_state_8_1), &id(slot_state_8_2), &id(slot_state_8_3), &id(slot_state_8_4),
      &id(slot_state_8_5), &id(slot_state_8_6), &id(slot_state_8_7), &id(slot_state_8_8) },
    { &id(slot_state_9_1), &id(slot_state_9_2), &id(slot_state_9_3), &id(slot_state_9_4),
      &id(slot_state_9_5), &id(slot_state_9_6), &id(slot_state_9_7), &id(slot_state_9_8) },
  };

  // -- Collect unique entity IDs + target pointers -----------
  // ordered_ids preserves insertion order so we can map results
  // back by position after splitting the response.
  std::vector<std::string>                      ordered_ids;
  std::map<std::string, std::vector<std::string*>> targets;

  for (int s = 0; s < 9; s++) {
    for (int e = 0; e < 8; e++) {
      char key[16]; slot_entity_key(key, s+1, e+1);
      std::string eid = nvs_read_str(key, "");
      if (eid.empty()) {
        *slot_states[s][e] = "";
        continue;
      }
      targets[eid].push_back(slot_states[s][e]);
      if (std::find(ordered_ids.begin(), ordered_ids.end(), eid)
          == ordered_ids.end())
        ordered_ids.push_back(eid);
    }
  }

  if (ordered_ids.empty()) {
    ESP_LOGD("alerts", "No entities configured -- skipping poll");
    return;
  }

  // -- Build Jinja2 template ----------------------------------
  // "{{ states('eid1') }}|{{ states('eid2') }}|..."
  // Pipe separator is safe — HA states never contain '|'
  std::string tmpl;
  for (const auto& eid : ordered_ids) {
    if (!tmpl.empty()) tmpl += "|";
    tmpl += "{{ states('" + eid + "') }}";
  }

  // -- Build JSON body ----------------------------------------
  // {"template": "..."}  must be JSON-encoded
  StaticJsonDocument<2048> req_doc;
  req_doc["template"] = tmpl;
  std::string req_body;
  serializeJson(req_doc, req_body);

  // -- Single POST to /api/template ---------------------------
  char url[128];
  snprintf(url, sizeof(url), "http://%s:%d/api/template",
           ha_ip.c_str(), ha_port);

  std::string auth = "Bearer " + ha_token;
  auto resp = http_request::global_http_request_component->post(
    url,
    { {"Authorization", auth.c_str()},
      {"Content-Type",  "application/json"} },
    req_body
  );

  if (!resp || resp->status_code != 200) {
    ESP_LOGW("alerts", "Template request failed (status %d)",
             resp ? resp->status_code : -1);
    // Mark all as unavailable so display shows nothing rather
    // than stale data from previous cycle
    for (auto& kv : targets)
      for (auto* t : kv.second) *t = "unavailable";
    return;
  }

  // -- Parse pipe-delimited response --------------------------
  // Response body is plain text: "armed_away|on|off|on|..."
  // Strip any surrounding whitespace/quotes that HA might add
  std::string body = resp->content;
  // Trim whitespace
  while (!body.empty() && (body.front() == ' ' || body.front() == '\n'
                         || body.front() == '\r')) body.erase(0,1);
  while (!body.empty() && (body.back()  == ' ' || body.back()  == '\n'
                         || body.back()  == '\r')) body.pop_back();

  // Split on '|' — one value per entity in ordered_ids
  std::vector<std::string> states_out;
  std::string token;
  for (char c : body) {
    if (c == '|') { states_out.push_back(token); token.clear(); }
    else token += c;
  }
  states_out.push_back(token);  // last segment after final |

  // -- Write states back into globals -------------------------
  for (size_t i = 0; i < ordered_ids.size(); i++) {
    std::string state = (i < states_out.size()) ? states_out[i] : "unknown";
    const auto& eid = ordered_ids[i];
    for (auto* t : targets[eid]) *t = state;
    ESP_LOGD("alerts", "[%zu] %s = %s", i, eid.c_str(), state.c_str());
  }

  ESP_LOGI("alerts", "Polled %zu entities in 1 request", ordered_ids.size());
}


// -- NVS namespace for all config keys -----------------------
static const char* NVS_NS = "kp_config";

// -- NVS helpers ---------------------------------------------
static void nvs_write_str(const char* key, const std::string& val) {
  nvs_handle_t h;
  if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
    nvs_set_str(h, key, val.c_str());
    nvs_commit(h);
    nvs_close(h);
  }
}

static std::string nvs_read_str(const char* key,
                                 const std::string& default_val = "") {
  nvs_handle_t h;
  if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return default_val;
  size_t len = 0;
  if (nvs_get_str(h, key, nullptr, &len) != ESP_OK) {
    nvs_close(h); return default_val;
  }
  std::string val(len, '\0');
  nvs_get_str(h, key, &val[0], &len);
  nvs_close(h);
  // Remove null terminator if present
  if (!val.empty() && val.back() == '\0') val.pop_back();
  return val;
}

static void nvs_write_int(const char* key, int val) {
  nvs_handle_t h;
  if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
    nvs_set_i32(h, key, val);
    nvs_commit(h);
    nvs_close(h);
  }
}

static int nvs_read_int(const char* key, int default_val = 0) {
  nvs_handle_t h;
  if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return default_val;
  int32_t val = default_val;
  nvs_get_i32(h, key, &val);
  nvs_close(h);
  return (int)val;
}

// -- CORS + JSON response helper -----------------------------
static esp_err_t send_json(httpd_req_t* req,
                            const std::string& body,
                            int status = 200) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods",
                     "GET, POST, OPTIONS");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers",
                     "Content-Type");
  if (status != 200) {
    char status_str[8];
    snprintf(status_str, sizeof(status_str), "%d", status);
    httpd_resp_set_status(req, status == 400 ? "400 Bad Request"
                                             : "500 Internal Error");
  }
  httpd_resp_send(req, body.c_str(), body.size());
  return ESP_OK;
}

// -- Read full request body ----------------------------------
static std::string read_body(httpd_req_t* req) {
  if (req->content_len == 0) return "";
  std::string body(req->content_len, '\0');
  int received = 0;
  while (received < (int)req->content_len) {
    int r = httpd_req_recv(req, &body[received],
                           req->content_len - received);
    if (r <= 0) break;
    received += r;
  }
  return body;
}

// -- OPTIONS preflight handler (CORS) -----------------------
static esp_err_t options_handler(httpd_req_t* req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods",
                     "GET, POST, OPTIONS");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers",
                     "Content-Type");
  httpd_resp_send(req, "", 0);
  return ESP_OK;
}

// ============================================================
//  GET /api/config  -- returns entire config as JSON
// ============================================================
static esp_err_t handle_get_config(httpd_req_t* req) {
  StaticJsonDocument<4096> doc;

  // WiFi (never return password)
  doc["wifi"]["ssid"] = nvs_read_str("wifi_ssid");

  // Home Assistant
  doc["ha"]["ip"]   = nvs_read_str("ha_ip", "192.168.1.10");
  doc["ha"]["port"] = nvs_read_int("ha_port", 8123);

  // Sensors
  doc["sensors"]["weather"]         = nvs_read_str("s_weather",   "weather.home");
  doc["sensors"]["solar_today"]     = nvs_read_str("s_solar",     "sensor.solar_energy_today");
  doc["sensors"]["solar_expected"]  = nvs_read_str("s_solar_exp", "sensor.solar_forecast_today");
  doc["sensors"]["load_today"]      = nvs_read_str("s_load",      "sensor.home_consumption_today");
  doc["sensors"]["grid_export"]     = nvs_read_str("s_gexport",   "sensor.grid_export_today");
  doc["sensors"]["grid_import"]     = nvs_read_str("s_gimport",   "sensor.grid_import_today");
  doc["sensors"]["battery_soc"]     = nvs_read_str("s_bat_soc",   "sensor.battery_soc");
  doc["sensors"]["battery_status"]  = nvs_read_str("s_bat_stat",  "sensor.battery_status");
  doc["sensors"]["battery_eta"]     = nvs_read_str("s_bat_eta",   "sensor.battery_time_to_full");
  doc["sensors"]["alarm"]           = nvs_read_str("s_alarm",     "alarm_control_panel.home");
  doc["sensors"]["door"]            = nvs_read_str("s_door",      "binary_sensor.any_door_open");
  doc["sensors"]["window"]          = nvs_read_str("s_window",    "binary_sensor.any_window_open");
  doc["sensors"]["plant"]           = nvs_read_str("s_plant",     "sensor.plant_watering_status");
  doc["sensors"]["heating"]         = nvs_read_str("s_heat",      "binary_sensor.heating_active");
  doc["sensors"]["hotwater"]        = nvs_read_str("s_hotwater",  "binary_sensor.hot_water_active");
  doc["sensors"]["vacuum1"]         = nvs_read_str("s_vac1",      "vacuum.downstairs");
  doc["sensors"]["vacuum2"]         = nvs_read_str("s_vac2",      "vacuum.upstairs");
  doc["sensors"]["mower"]           = nvs_read_str("s_mower",     "lawn_mower.garden");
  doc["sensors"]["aqi"]             = nvs_read_str("s_aqi",       "sensor.air_quality_index");
  doc["sensors"]["calendar"]        = nvs_read_str("s_cal",       "sensor.panel_cal_line_1");
  doc["sensors"]["heatpump"]        = nvs_read_str("s_heatpump",  "climate.heat_pump");

  // Thresholds
  doc["thresholds"]["battery_critical"]  = nvs_read_int("t_bat_crit",   10);
  doc["thresholds"]["battery_low"]       = nvs_read_int("t_bat_low",    20);
  doc["thresholds"]["solar_peak"]        = nvs_read_int("t_solar_peak", 52);
  doc["thresholds"]["transport_urgent"]  = nvs_read_int("t_transport",   5);
  doc["thresholds"]["motion_recent"]     = nvs_read_int("t_motion",     30);
  doc["thresholds"]["climate_tolerance"] = nvs_read_int("t_climate",     1);
  doc["thresholds"]["config_timeout"]    = nvs_read_int("t_cfg_timeout",600);
  doc["thresholds"]["longpress_ms"]      = nvs_read_int("t_longpress",  800);

  // Colors (0=black 1=black 2=red 3=yellow 4=green 5=blue)
  doc["colors"]["alarm_inactive"]    = nvs_read_int("c_alarm_off",   1);
  doc["colors"]["alarm_disarmed"]    = nvs_read_int("c_alarm_dis",   4);
  doc["colors"]["alarm_armed"]       = nvs_read_int("c_alarm_arm",   2);
  doc["colors"]["alarm_arming"]      = nvs_read_int("c_alarm_arming",3);
  doc["colors"]["alarm_triggered"]   = nvs_read_int("c_alarm_trig",  2);
  doc["colors"]["door_inactive"]     = nvs_read_int("c_door_off",    1);
  doc["colors"]["door_open"]         = nvs_read_int("c_door_open",   2);
  doc["colors"]["window_inactive"]   = nvs_read_int("c_win_off",     1);
  doc["colors"]["window_open"]       = nvs_read_int("c_win_open",    5);
  doc["colors"]["plant_inactive"]    = nvs_read_int("c_plant_off",   1);
  doc["colors"]["plant_ok"]          = nvs_read_int("c_plant_ok",    1);
  doc["colors"]["plant_due"]         = nvs_read_int("c_plant_due",   2);
  doc["colors"]["plant_watered"]     = nvs_read_int("c_plant_watered",4);
  doc["colors"]["heating_inactive"]  = nvs_read_int("c_heat_off",    1);
  doc["colors"]["heating_active"]    = nvs_read_int("c_heat_on",     2);
  doc["colors"]["hotwater_inactive"] = nvs_read_int("c_hw_off",      1);
  doc["colors"]["hotwater_active"]   = nvs_read_int("c_hw_on",       2);
  doc["colors"]["vac1_inactive"]     = nvs_read_int("c_vac1_off",    1);
  doc["colors"]["vac1_running"]      = nvs_read_int("c_vac1_run",    4);
  doc["colors"]["vac1_scheduled"]    = nvs_read_int("c_vac1_sched",  5);
  doc["colors"]["vac1_error"]        = nvs_read_int("c_vac1_err",    2);
  doc["colors"]["vac2_inactive"]     = nvs_read_int("c_vac2_off",    1);
  doc["colors"]["vac2_running"]      = nvs_read_int("c_vac2_run",    4);
  doc["colors"]["vac2_scheduled"]    = nvs_read_int("c_vac2_sched",  5);
  doc["colors"]["vac2_error"]        = nvs_read_int("c_vac2_err",    2);
  doc["colors"]["mower_inactive"]    = nvs_read_int("c_mow_off",     1);
  doc["colors"]["mower_running"]     = nvs_read_int("c_mow_run",     4);
  doc["colors"]["mower_scheduled"]   = nvs_read_int("c_mow_sched",   5);
  doc["colors"]["mower_error"]       = nvs_read_int("c_mow_err",     2);
  doc["colors"]["battery_charging"]  = nvs_read_int("c_bat_chg",     4);
  doc["colors"]["battery_full"]      = nvs_read_int("c_bat_full",    4);
  doc["colors"]["battery_discharging"]= nvs_read_int("c_bat_dis",    3);
  doc["colors"]["battery_critical"]  = nvs_read_int("c_bat_crit",    2);
  doc["colors"]["battery_idle"]      = nvs_read_int("c_bat_idle",    1);

  // Icons
  doc["icons"]["alarm"]    = nvs_read_str("i_alarm",   "shield-home");
  doc["icons"]["door"]     = nvs_read_str("i_door",    "door-open");
  doc["icons"]["window"]   = nvs_read_str("i_window",  "window-open");
  doc["icons"]["plant"]    = nvs_read_str("i_plant",   "flower");
  doc["icons"]["heating"]  = nvs_read_str("i_heating", "radiator");
  doc["icons"]["hotwater"] = nvs_read_str("i_hotwater","water-boiler");
  doc["icons"]["vacuum1"]  = nvs_read_str("i_vac1",    "robot-vacuum");
  doc["icons"]["vacuum2"]  = nvs_read_str("i_vac2",    "robot-vacuum-variant");
  doc["icons"]["mower"]    = nvs_read_str("i_mower",   "robot-mower");

  // Device status
  doc["status"]["ip"]      = WiFi.localIP().toString().c_str();
  doc["status"]["rssi"]    = WiFi.RSSI();
  doc["status"]["uptime"]  = (int)(millis() / 1000);
  doc["status"]["version"] = ESPHOME_VERSION;

  // Alert slots
  for (int i = 1; i <= 9; i++) {
    char key_label[16], key_cond[16], key_value[16], key_enabled[16];
    snprintf(key_label,   sizeof(key_label),   "a_label_%d", i);
    snprintf(key_cond,    sizeof(key_cond),     "a_cond_%d",  i);
    snprintf(key_value,   sizeof(key_value),    "a_value_%d", i);
    snprintf(key_enabled, sizeof(key_enabled),  "a_en_%d",    i);

    char slot_key[8];
    snprintf(slot_key, sizeof(slot_key), "slot%d", i);
    doc["alerts"][slot_key]["label"]   = nvs_read_str(key_label,   "");
    doc["alerts"][slot_key]["cond"]    = nvs_read_str(key_cond,    "eq");
    doc["alerts"][slot_key]["value"]   = nvs_read_str(key_value,   "on");
    doc["alerts"][slot_key]["enabled"] = nvs_read_int(key_enabled, 1);
  }

  std::string output;
  serializeJson(doc, output);
  return send_json(req, output);
}

// ============================================================
//  POST /api/config/wifi
// ============================================================
static esp_err_t handle_post_wifi(httpd_req_t* req) {
  std::string body = read_body(req);
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
    return send_json(req, R"({"ok":false,"error":"invalid JSON"})", 400);

  if (doc.containsKey("ssid"))
    nvs_write_str("wifi_ssid", doc["ssid"].as<std::string>());
  if (doc.containsKey("password"))
    nvs_write_str("wifi_pass", doc["password"].as<std::string>());
  if (doc.containsKey("ap_password"))
    nvs_write_str("ap_password", doc["ap_password"].as<std::string>());

  // Apply to ESPHome wifi component at next reboot
  return send_json(req, R"({"ok":true,"note":"reboot to apply"})");
}

// ============================================================
//  POST /api/config/ha
// ============================================================
static esp_err_t handle_post_ha(httpd_req_t* req) {
  std::string body = read_body(req);
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
    return send_json(req, R"({"ok":false,"error":"invalid JSON"})", 400);

  if (doc.containsKey("ip"))    nvs_write_str("ha_ip",    doc["ip"]);
  if (doc.containsKey("port"))  nvs_write_int("ha_port",  doc["port"]);
  if (doc.containsKey("token")) nvs_write_str("ha_token", doc["token"]);

  return send_json(req, R"({"ok":true})");
}

// ============================================================
//  POST /api/config/sensors
// ============================================================
static esp_err_t handle_post_sensors(httpd_req_t* req) {
  std::string body = read_body(req);
  StaticJsonDocument<2048> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
    return send_json(req, R"({"ok":false,"error":"invalid JSON"})", 400);

  // Map JSON key -> NVS key
  const std::pair<const char*, const char*> sensor_map[] = {
    {"weather",        "s_weather"},
    {"solar_today",    "s_solar"},
    {"solar_expected", "s_solar_exp"},
    {"load_today",     "s_load"},
    {"grid_export",    "s_gexport"},
    {"grid_import",    "s_gimport"},
    {"battery_soc",    "s_bat_soc"},
    {"battery_status", "s_bat_stat"},
    {"battery_eta",    "s_bat_eta"},
    {"alarm",          "s_alarm"},
    {"door",           "s_door"},
    {"window",         "s_window"},
    {"plant",          "s_plant"},
    {"heating",        "s_heat"},
    {"hotwater",       "s_hotwater"},
    {"vacuum1",        "s_vac1"},
    {"vacuum2",        "s_vac2"},
    {"mower",          "s_mower"},
    {"aqi",            "s_aqi"},
    {"calendar",       "s_cal"},
    {"heatpump",       "s_heatpump"},
  };

  for (auto& kv : sensor_map) {
    if (doc.containsKey(kv.first))
      nvs_write_str(kv.second, doc[kv.first].as<std::string>());
  }

  // Also update ESPHome globals for entity IDs so display picks them up
  // without reboot (entity subscriptions require reboot, but text is
  // used by display lambda directly)
  if (doc.containsKey("solar_today"))
    id(g_solar_entity) = doc["solar_today"].as<std::string>();

  return send_json(req, R"({"ok":true})");
}

// ============================================================
//  POST /api/config/colors
// ============================================================
static esp_err_t handle_post_colors(httpd_req_t* req) {
  std::string body = read_body(req);
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
    return send_json(req, R"({"ok":false,"error":"invalid JSON"})", 400);

  const std::pair<const char*, const char*> color_map[] = {
    {"alarm_inactive",     "c_alarm_off"},
    {"alarm_disarmed",     "c_alarm_dis"},
    {"alarm_armed",        "c_alarm_arm"},
    {"alarm_arming",       "c_alarm_arming"},
    {"alarm_triggered",    "c_alarm_trig"},
    {"door_inactive",      "c_door_off"},
    {"door_open",          "c_door_open"},
    {"window_inactive",    "c_win_off"},
    {"window_open",        "c_win_open"},
    {"plant_inactive",     "c_plant_off"},
    {"plant_ok",           "c_plant_ok"},
    {"plant_due",          "c_plant_due"},
    {"plant_watered",      "c_plant_watered"},
    {"heating_inactive",   "c_heat_off"},
    {"heating_active",     "c_heat_on"},
    {"hotwater_inactive",  "c_hw_off"},
    {"hotwater_active",    "c_hw_on"},
    {"vac1_inactive",      "c_vac1_off"},
    {"vac1_running",       "c_vac1_run"},
    {"vac1_scheduled",     "c_vac1_sched"},
    {"vac1_error",         "c_vac1_err"},
    {"vac2_inactive",      "c_vac2_off"},
    {"vac2_running",       "c_vac2_run"},
    {"vac2_scheduled",     "c_vac2_sched"},
    {"vac2_error",         "c_vac2_err"},
    {"mower_inactive",     "c_mow_off"},
    {"mower_running",      "c_mow_run"},
    {"mower_scheduled",    "c_mow_sched"},
    {"mower_error",        "c_mow_err"},
    {"battery_charging",   "c_bat_chg"},
    {"battery_full",       "c_bat_full"},
    {"battery_discharging","c_bat_dis"},
    {"battery_critical",   "c_bat_crit"},
    {"battery_idle",       "c_bat_idle"},
  };

  for (auto& kv : color_map) {
    if (doc.containsKey(kv.first)) {
      int val = doc[kv.first].as<int>();
      nvs_write_int(kv.second, val);

      // Also update live ESPHome globals so display reflects immediately
      if      (strcmp(kv.first, "alarm_inactive")     == 0) id(color_alarm_inactive)    = val;
      else if (strcmp(kv.first, "alarm_disarmed")     == 0) id(color_alarm_disarmed)     = val;
      else if (strcmp(kv.first, "alarm_armed")        == 0) id(color_alarm_armed)        = val;
      else if (strcmp(kv.first, "alarm_arming")       == 0) id(color_alarm_arming)       = val;
      else if (strcmp(kv.first, "alarm_triggered")    == 0) id(color_alarm_triggered)    = val;
      else if (strcmp(kv.first, "door_inactive")      == 0) id(color_door_inactive)      = val;
      else if (strcmp(kv.first, "door_open")          == 0) id(color_door_open)          = val;
      else if (strcmp(kv.first, "window_inactive")    == 0) id(color_window_inactive)    = val;
      else if (strcmp(kv.first, "window_open")        == 0) id(color_window_open)        = val;
      else if (strcmp(kv.first, "plant_inactive")     == 0) id(color_plant_inactive)     = val;
      else if (strcmp(kv.first, "plant_ok")           == 0) id(color_plant_ok)           = val;
      else if (strcmp(kv.first, "plant_due")          == 0) id(color_plant_due)          = val;
      else if (strcmp(kv.first, "plant_watered")      == 0) id(color_plant_watered)      = val;
      else if (strcmp(kv.first, "heating_inactive")   == 0) id(color_heating_inactive)   = val;
      else if (strcmp(kv.first, "heating_active")     == 0) id(color_heating_active)     = val;
      else if (strcmp(kv.first, "hotwater_inactive")  == 0) id(color_hotwater_inactive)  = val;
      else if (strcmp(kv.first, "hotwater_active")    == 0) id(color_hotwater_active)    = val;
      else if (strcmp(kv.first, "vac1_inactive")      == 0) id(color_vac1_inactive)      = val;
      else if (strcmp(kv.first, "vac1_running")       == 0) id(color_vac1_running)       = val;
      else if (strcmp(kv.first, "vac1_scheduled")     == 0) id(color_vac1_scheduled)     = val;
      else if (strcmp(kv.first, "vac1_error")         == 0) id(color_vac1_error)         = val;
      else if (strcmp(kv.first, "vac2_inactive")      == 0) id(color_vac2_inactive)      = val;
      else if (strcmp(kv.first, "vac2_running")       == 0) id(color_vac2_running)       = val;
      else if (strcmp(kv.first, "vac2_scheduled")     == 0) id(color_vac2_scheduled)     = val;
      else if (strcmp(kv.first, "vac2_error")         == 0) id(color_vac2_error)         = val;
      else if (strcmp(kv.first, "mower_inactive")     == 0) id(color_mower_inactive)     = val;
      else if (strcmp(kv.first, "mower_running")      == 0) id(color_mower_running)      = val;
      else if (strcmp(kv.first, "mower_scheduled")    == 0) id(color_mower_scheduled)    = val;
      else if (strcmp(kv.first, "mower_error")        == 0) id(color_mower_error)        = val;
      else if (strcmp(kv.first, "battery_charging")   == 0) id(color_battery_charging)   = val;
      else if (strcmp(kv.first, "battery_full")       == 0) id(color_battery_full)       = val;
      else if (strcmp(kv.first, "battery_discharging")== 0) id(color_battery_discharging)= val;
      else if (strcmp(kv.first, "battery_critical")   == 0) id(color_battery_critical)   = val;
      else if (strcmp(kv.first, "battery_idle")       == 0) id(color_battery_idle)       = val;
    }
  }

  return send_json(req, R"({"ok":true})");
}

// ============================================================
//  POST /api/config/thresholds
// ============================================================
static esp_err_t handle_post_thresholds(httpd_req_t* req) {
  std::string body = read_body(req);
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
    return send_json(req, R"({"ok":false,"error":"invalid JSON"})", 400);

  if (doc.containsKey("battery_critical")) {
    int v = doc["battery_critical"]; nvs_write_int("t_bat_crit", v);
    id(battery_critical_pct) = v;
  }
  if (doc.containsKey("battery_low")) {
    int v = doc["battery_low"]; nvs_write_int("t_bat_low", v);
    id(battery_low_pct) = v;
  }
  if (doc.containsKey("solar_peak")) {
    int v = doc["solar_peak"]; nvs_write_int("t_solar_peak", v);
    id(solar_peak_expected) = (float)v;
  }
  if (doc.containsKey("transport_urgent")) {
    int v = doc["transport_urgent"]; nvs_write_int("t_transport", v);
    id(transport_urgent_mins) = v;
  }
  if (doc.containsKey("motion_recent")) {
    int v = doc["motion_recent"]; nvs_write_int("t_motion", v);
    id(motion_recent_mins) = v;
  }
  if (doc.containsKey("climate_tolerance")) {
    int v = doc["climate_tolerance"]; nvs_write_int("t_climate", v);
    id(climate_tolerance) = v;
  }
  if (doc.containsKey("config_timeout")) {
    int v = doc["config_timeout"]; nvs_write_int("t_cfg_timeout", v);
    id(config_timeout_seconds) = v;
  }
  if (doc.containsKey("longpress_ms")) {
    int v = doc["longpress_ms"]; nvs_write_int("t_longpress", v);
    id(btn_middle_long_ms) = v;
  }

  return send_json(req, R"({"ok":true})");
}

// ============================================================
//  POST /api/config/icons
// ============================================================
static esp_err_t handle_post_icons(httpd_req_t* req) {
  std::string body = read_body(req);
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
    return send_json(req, R"({"ok":false,"error":"invalid JSON"})", 400);

  const std::pair<const char*, const char*> icon_map[] = {
    {"alarm",    "i_alarm"},
    {"door",     "i_door"},
    {"window",   "i_window"},
    {"plant",    "i_plant"},
    {"heating",  "i_heating"},
    {"hotwater", "i_hotwater"},
    {"vacuum1",  "i_vac1"},
    {"vacuum2",  "i_vac2"},
    {"mower",    "i_mower"},
  };

  for (auto& kv : icon_map) {
    if (doc.containsKey(kv.first))
      nvs_write_str(kv.second, doc[kv.first].as<std::string>());
  }

  return send_json(req, R"({"ok":true,"note":"icon changes apply on next refresh"})");
}

// ============================================================
//  POST /api/action/refresh  -- force display update
// ============================================================
static esp_err_t handle_action_refresh(httpd_req_t* req) {
  // Schedule refresh — can't call ESPHome component directly from
  // HTTP handler thread, so set a flag that the main loop picks up
  id(force_refresh_flag) = true;
  return send_json(req, R"({"ok":true})");
}

// ============================================================
//  POST /api/action/restart  -- reboot device
// ============================================================
static esp_err_t handle_action_restart(httpd_req_t* req) {
  send_json(req, R"({"ok":true,"note":"rebooting"})");
  delay(200);
  esp_restart();
  return ESP_OK;
}

// ============================================================
//  POST /api/config/alerts  -- save alert slot config
// ============================================================
static esp_err_t handle_post_alerts(httpd_req_t* req) {
  std::string body = read_body(req);
  StaticJsonDocument<2048> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
    return send_json(req, R"({"ok":false,"error":"invalid JSON"})", 400);

  // Expects: { "1": {label, cond, value, enabled}, "2": {...}, ... }
  for (int i = 1; i <= 9; i++) {
    char slot_key[4];
    snprintf(slot_key, sizeof(slot_key), "%d", i);
    if (!doc.containsKey(slot_key)) continue;

    JsonObject slot = doc[slot_key];
    char key_label[16], key_cond[16], key_value[16], key_enabled[16];
    snprintf(key_label,   sizeof(key_label),   "a_label_%d", i);
    snprintf(key_cond,    sizeof(key_cond),     "a_cond_%d",  i);
    snprintf(key_value,   sizeof(key_value),    "a_value_%d", i);
    snprintf(key_enabled, sizeof(key_enabled),  "a_en_%d",    i);

    if (slot.containsKey("label"))   nvs_write_str(key_label,  slot["label"].as<std::string>());
    if (slot.containsKey("cond"))    nvs_write_str(key_cond,   slot["cond"].as<std::string>());
    if (slot.containsKey("value"))   nvs_write_str(key_value,  slot["value"].as<std::string>());
    if (slot.containsKey("enabled")) nvs_write_int(key_enabled, slot["enabled"].as<int>());

    // Update live globals immediately
    switch (i) {
      case 1: if (slot.containsKey("label"))   id(alert_label_1)   = slot["label"].as<std::string>();
              if (slot.containsKey("cond"))    id(alert_cond_1)    = slot["cond"].as<std::string>();
              if (slot.containsKey("value"))   id(alert_value_1)   = slot["value"].as<std::string>();
              if (slot.containsKey("enabled")) id(alert_enabled_1) = slot["enabled"]; break;
      case 2: if (slot.containsKey("label"))   id(alert_label_2)   = slot["label"].as<std::string>();
              if (slot.containsKey("cond"))    id(alert_cond_2)    = slot["cond"].as<std::string>();
              if (slot.containsKey("value"))   id(alert_value_2)   = slot["value"].as<std::string>();
              if (slot.containsKey("enabled")) id(alert_enabled_2) = slot["enabled"]; break;
      case 3: if (slot.containsKey("label"))   id(alert_label_3)   = slot["label"].as<std::string>();
              if (slot.containsKey("cond"))    id(alert_cond_3)    = slot["cond"].as<std::string>();
              if (slot.containsKey("value"))   id(alert_value_3)   = slot["value"].as<std::string>();
              if (slot.containsKey("enabled")) id(alert_enabled_3) = slot["enabled"]; break;
      case 4: if (slot.containsKey("label"))   id(alert_label_4)   = slot["label"].as<std::string>();
              if (slot.containsKey("cond"))    id(alert_cond_4)    = slot["cond"].as<std::string>();
              if (slot.containsKey("value"))   id(alert_value_4)   = slot["value"].as<std::string>();
              if (slot.containsKey("enabled")) id(alert_enabled_4) = slot["enabled"]; break;
      case 5: if (slot.containsKey("label"))   id(alert_label_5)   = slot["label"].as<std::string>();
              if (slot.containsKey("cond"))    id(alert_cond_5)    = slot["cond"].as<std::string>();
              if (slot.containsKey("value"))   id(alert_value_5)   = slot["value"].as<std::string>();
              if (slot.containsKey("enabled")) id(alert_enabled_5) = slot["enabled"]; break;
      case 6: if (slot.containsKey("label"))   id(alert_label_6)   = slot["label"].as<std::string>();
              if (slot.containsKey("cond"))    id(alert_cond_6)    = slot["cond"].as<std::string>();
              if (slot.containsKey("value"))   id(alert_value_6)   = slot["value"].as<std::string>();
              if (slot.containsKey("enabled")) id(alert_enabled_6) = slot["enabled"]; break;
      case 7: if (slot.containsKey("label"))   id(alert_label_7)   = slot["label"].as<std::string>();
              if (slot.containsKey("cond"))    id(alert_cond_7)    = slot["cond"].as<std::string>();
              if (slot.containsKey("value"))   id(alert_value_7)   = slot["value"].as<std::string>();
              if (slot.containsKey("enabled")) id(alert_enabled_7) = slot["enabled"]; break;
      case 8: if (slot.containsKey("label"))   id(alert_label_8)   = slot["label"].as<std::string>();
              if (slot.containsKey("cond"))    id(alert_cond_8)    = slot["cond"].as<std::string>();
              if (slot.containsKey("value"))   id(alert_value_8)   = slot["value"].as<std::string>();
              if (slot.containsKey("enabled")) id(alert_enabled_8) = slot["enabled"]; break;
      case 9: if (slot.containsKey("label"))   id(alert_label_9)   = slot["label"].as<std::string>();
              if (slot.containsKey("cond"))    id(alert_cond_9)    = slot["cond"].as<std::string>();
              if (slot.containsKey("value"))   id(alert_value_9)   = slot["value"].as<std::string>();
              if (slot.containsKey("enabled")) id(alert_enabled_9) = slot["enabled"]; break;
    }
  }
  return send_json(req, R"({"ok":true})");
}

// ============================================================
//  Register all handlers on the existing httpd server
//  Call this from esphome on_boot after web_server starts
// ============================================================
void register_config_api(httpd_handle_t server) {
  auto reg = [&](const char* uri, httpd_method_t method,
                 esp_err_t (*handler)(httpd_req_t*)) {
    httpd_uri_t h = { uri, method, handler, nullptr };
    httpd_register_uri_handler(server, &h);
  };

  // OPTIONS preflight for all /api/ routes
  reg("/api/*",                  HTTP_OPTIONS, options_handler);

  // Config GET
  reg("/api/config",             HTTP_GET,  handle_get_config);

  // Config POST
  reg("/api/config/wifi",        HTTP_POST, handle_post_wifi);
  reg("/api/config/ha",          HTTP_POST, handle_post_ha);
  reg("/api/config/sensors",     HTTP_POST, handle_post_sensors);
  reg("/api/config/colors",      HTTP_POST, handle_post_colors);
  reg("/api/config/thresholds",  HTTP_POST, handle_post_thresholds);
  reg("/api/config/icons",       HTTP_POST, handle_post_icons);

  // Actions
  reg("/api/config/alerts",      HTTP_POST, handle_post_alerts);
  reg("/api/action/refresh",     HTTP_POST, handle_action_refresh);
  reg("/api/action/restart",     HTTP_POST, handle_action_restart);

  ESP_LOGI("config_api", "Config API registered on /api/*");
}

// ============================================================
//  Load all NVS config into ESPHome globals on boot
//  Call this from on_boot after NVS is initialised
// ============================================================
void load_config_from_nvs() {
  // Colors
  id(color_alarm_inactive)     = nvs_read_int("c_alarm_off",    1);
  id(color_alarm_disarmed)     = nvs_read_int("c_alarm_dis",    4);
  id(color_alarm_armed)        = nvs_read_int("c_alarm_arm",    2);
  id(color_alarm_arming)       = nvs_read_int("c_alarm_arming", 3);
  id(color_alarm_triggered)    = nvs_read_int("c_alarm_trig",   2);
  id(color_door_inactive)      = nvs_read_int("c_door_off",     1);
  id(color_door_open)          = nvs_read_int("c_door_open",    2);
  id(color_window_inactive)    = nvs_read_int("c_win_off",      1);
  id(color_window_open)        = nvs_read_int("c_win_open",     5);
  id(color_plant_inactive)     = nvs_read_int("c_plant_off",    1);
  id(color_plant_ok)           = nvs_read_int("c_plant_ok",     1);
  id(color_plant_due)          = nvs_read_int("c_plant_due",    2);
  id(color_plant_watered)      = nvs_read_int("c_plant_watered",4);
  id(color_heating_inactive)   = nvs_read_int("c_heat_off",     1);
  id(color_heating_active)     = nvs_read_int("c_heat_on",      2);
  id(color_hotwater_inactive)  = nvs_read_int("c_hw_off",       1);
  id(color_hotwater_active)    = nvs_read_int("c_hw_on",        2);
  id(color_vac1_inactive)      = nvs_read_int("c_vac1_off",     1);
  id(color_vac1_running)       = nvs_read_int("c_vac1_run",     4);
  id(color_vac1_scheduled)     = nvs_read_int("c_vac1_sched",   5);
  id(color_vac1_error)         = nvs_read_int("c_vac1_err",     2);
  id(color_vac2_inactive)      = nvs_read_int("c_vac2_off",     1);
  id(color_vac2_running)       = nvs_read_int("c_vac2_run",     4);
  id(color_vac2_scheduled)     = nvs_read_int("c_vac2_sched",   5);
  id(color_vac2_error)         = nvs_read_int("c_vac2_err",     2);
  id(color_mower_inactive)     = nvs_read_int("c_mow_off",      1);
  id(color_mower_running)      = nvs_read_int("c_mow_run",      4);
  id(color_mower_scheduled)    = nvs_read_int("c_mow_sched",    5);
  id(color_mower_error)        = nvs_read_int("c_mow_err",      2);
  id(color_battery_charging)   = nvs_read_int("c_bat_chg",      4);
  id(color_battery_full)       = nvs_read_int("c_bat_full",     4);
  id(color_battery_discharging)= nvs_read_int("c_bat_dis",      3);
  id(color_battery_critical)   = nvs_read_int("c_bat_crit",     2);
  id(color_battery_idle)       = nvs_read_int("c_bat_idle",     1);

  // Thresholds
  id(battery_critical_pct)   = nvs_read_int("t_bat_crit",    10);
  id(battery_low_pct)        = nvs_read_int("t_bat_low",     20);
  id(solar_peak_expected)    = (float)nvs_read_int("t_solar_peak", 52);
  id(transport_urgent_mins)  = nvs_read_int("t_transport",    5);
  id(motion_recent_mins)     = nvs_read_int("t_motion",      30);
  id(config_timeout_seconds) = nvs_read_int("t_cfg_timeout", 600);
  id(btn_middle_long_ms)     = nvs_read_int("t_longpress",   800);

  ESP_LOGI("config_api", "Config loaded from NVS");
}

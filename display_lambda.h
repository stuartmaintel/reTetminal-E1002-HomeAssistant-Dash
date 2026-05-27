// ============================================================
//  display_lambda.h
//  Kitchen Panel — reTerminal E1002
//  Drawing functions for all 3 screens
//
//  Include this file from kitchen-panel.yaml display lambdas.
//  Spectra 6 palette: BLACK, WHITE, RED, YELLOW, GREEN, BLUE
// ============================================================

#pragma once
#include "esphome.h"

// -- DISPLAY DIMENSIONS --------------------------------------
static const int SCREEN_W     = 800;
static const int SCREEN_H     = 480;
static const int LEFT_BAR_W   = 240;
static const int RIGHT_X      = LEFT_BAR_W;         // 240
static const int RIGHT_W      = SCREEN_W - LEFT_BAR_W; // 560
static const int ICON_BAR_H   = 44;
static const int DETAIL_Y     = ICON_BAR_H;
static const int DETAIL_H     = SCREEN_H - ICON_BAR_H;
static const int FOOTER_H     = 16;

// -- WEATHER ICON CODEPOINTS (MDI) ---------------------------
// Returns the MDI glyph char for a HA weather condition string
static const char* weather_icon(const std::string& cond) {
  if (cond == "sunny")                  return "\U000F059D";
  if (cond == "clear-night")            return "\U000F0595";
  if (cond == "partlycloudy")           return "\U000F0F31";
  if (cond == "partly-cloudy-day")      return "\U000F0F31";
  if (cond == "partly-cloudy-night")    return "\U000F0F31";
  if (cond == "cloudy")                 return "\U000F0590";
  if (cond == "rainy")                  return "\U000F0598";
  if (cond == "pouring")                return "\U000F0597";
  if (cond == "snowy")                  return "\U000F0599";
  if (cond == "fog")                    return "\U000F0591";
  if (cond == "hail")                   return "\U000F0592";
  if (cond == "lightning")              return "\U000F0593";
  if (cond == "lightning-rainy")        return "\U000F0593";
  return "\U000F0590";  // default: cloudy
}

// Weather icon color for Spectra 6
static Color weather_icon_color(const std::string& cond) {
  if (cond == "sunny" || cond == "partly-cloudy-day") return COLOR_YELLOW;
  if (cond == "rainy" || cond == "pouring")           return COLOR_BLUE;
  if (cond == "snowy")                                return COLOR_BLUE;
  if (cond == "clear-night")                          return COLOR_BLUE;
  if (cond == "lightning" || cond == "lightning-rainy") return COLOR_YELLOW;
  return COLOR_BLACK;  // cloudy, fog, hail, default
}

// -- COLOR INDEX TO SPECTRA COLOR ----------------------------
// Maps stored integer index to actual Spectra 6 Color object.
// 0=black(inactive) 1=black 2=red 3=yellow 4=green 5=blue
// NOTE: Spectra 6 has NO greyscale. Index 0 renders as black
// (used for inactive icons — visible but without color emphasis).
static Color idx_color(int idx) {
  switch (idx) {
    case 2:  return COLOR_RED;
    case 3:  return COLOR_YELLOW;
    case 4:  return COLOR_GREEN;
    case 5:  return COLOR_BLUE;
    default: return COLOR_BLACK;  // 0, 1, and fallback all = black
  }
}

// -- BATTERY HELPERS ------------------------------------------
// Battery bar color — reads from NVS color globals
static Color battery_bar_color(float pct, const std::string& status,
                                int critical_pct) {
  if (pct <= critical_pct)        return idx_color(id(color_battery_critical));
  if (status == "charging")       return idx_color(id(color_battery_charging));
  if (status == "full")           return idx_color(id(color_battery_full));
  if (status == "discharging")    return idx_color(id(color_battery_discharging));
  return idx_color(id(color_battery_idle));
}

// Battery MDI icon by %
static const char* battery_icon(float pct, const std::string& status) {
  if (status == "charging") return "\U000F007C";    // mdi:battery-charging-80
  if (pct >= 90)            return "\U000F0079";    // mdi:battery (full)
  if (pct >= 70)            return "\U000F004E";    // mdi:battery-80
  if (pct >= 50)            return "\U000F004B";    // mdi:battery-50
  if (pct >= 30)            return "\U000F0048";    // mdi:battery-30
  if (pct >= 10)            return "\U000F0045";    // mdi:battery-10
  return "\U000F0008";                              // mdi:battery-alert
}

// -- DRAW HELPERS ---------------------------------------------
// Draw a horizontal fill bar at (x,y) with given width, height, fill%, color
static void draw_fill_bar(display::Display& it,
                           int x, int y, int w, int h,
                           float pct, Color fill_color) {
  // Outline
  it.rectangle(x, y, w, h, COLOR_BLACK);
  // Fill
  int fill_w = (int)((w - 4) * clamp(pct / 100.0f, 0.0f, 1.0f));
  if (fill_w > 0) {
    it.filled_rectangle(x + 2, y + 2, fill_w, h - 4, fill_color);
  }
}

// Draw a single icon + value row (icon left, text right)
static void draw_solar_cell(display::Display& it,
                             int x, int y, int w, int h,
                             const char* icon_glyph,
                             Color icon_color,
                             const char* value,
                             const char* sublabel) {
  it.rectangle(x, y, w, h, COLOR_BLACK);
  // Icon — centered top half
  it.print(x + w/2, y + 8, id(font_icons_32), icon_color,
           display::TextAlign::TOP_CENTER, icon_glyph);
  // Value
  it.print(x + w/2, y + 44, id(font_medium), COLOR_BLACK,
           display::TextAlign::TOP_CENTER, value);
  // Sub-label
  it.print(x + w/2, y + 68, id(font_mono_small), COLOR_GREY,
           display::TextAlign::TOP_CENTER, sublabel);
}

// -- QR CODE GENERATION ---------------------------------------
// Simple URL QR using a pre-built 21×21 matrix for http://x.x.x.x
// For a real device, generate QR data server-side or embed qrcode lib.
// Here we draw the finder patterns and placeholders; the IP is printed
// below the QR for manual entry as a reliable fallback.
static void draw_qr_placeholder(display::Display& it, int cx, int cy,
                                 int size, const std::string& ip) {
  // Each module = size/21 px
  int mod = size / 21;
  int ox  = cx - size / 2;
  int oy  = cy - size / 2;

  // Background
  it.filled_rectangle(ox - 4, oy - 4, size + 8, size + 8, COLOR_WHITE);
  it.rectangle(ox - 4, oy - 4, size + 8, size + 8, COLOR_BLACK);

  // Fill entire area white first
  it.filled_rectangle(ox, oy, size, size, COLOR_WHITE);

  auto draw_mod = [&](int col, int row) {
    it.filled_rectangle(ox + col * mod, oy + row * mod, mod, mod, COLOR_BLACK);
  };

  // Top-left finder 7×7
  for (int r = 0; r < 7; r++) for (int c = 0; c < 7; c++) {
    if (r==0||r==6||c==0||c==6||(r>=2&&r<=4&&c>=2&&c<=4)) draw_mod(c, r);
  }
  // Top-right finder
  for (int r = 0; r < 7; r++) for (int c = 14; c < 21; c++) {
    int lc = c - 14;
    if (r==0||r==6||lc==0||lc==6||(r>=2&&r<=4&&lc>=2&&lc<=4)) draw_mod(c, r);
  }
  // Bottom-left finder
  for (int r = 14; r < 21; r++) for (int c = 0; c < 7; c++) {
    int lr = r - 14;
    if (lr==0||lr==6||c==0||c==6||(lr>=2&&lr<=4&&c>=2&&c<=4)) draw_mod(c, r);
  }

  // Timing strips
  for (int i = 8; i < 13; i += 2) { draw_mod(i, 6); draw_mod(6, i); }

  // Pseudo-random data modules in the data region (visual only)
  // Replace with real QR library output for production
  int data[][2] = {
    {8,8},{9,8},{11,8},{13,8},{8,9},{10,9},{12,9},{9,10},{11,10},
    {13,10},{8,11},{10,11},{8,12},{9,12},{12,12},{8,13},{11,13},
    {15,8},{17,8},{19,8},{16,9},{18,9},{20,9},{15,10},{17,10},
    {19,11},{16,11},{15,12},{18,12},{20,12},{8,15},{10,15},
    {12,15},{9,16},{11,16},{13,16},{8,17},{10,17},{12,18},
    {9,18},{11,19},{13,19},{8,20},{10,20}
  };
  for (auto& d : data) draw_mod(d[0], d[1]);

  // IP address printed below QR
  char url[32];
  snprintf(url, sizeof(url), "http://%s", ip.c_str());
  it.print(cx, oy + size + 10, id(font_small), COLOR_BLACK,
           display::TextAlign::TOP_CENTER, url);
}

// -- ICON BAR SLOT ---------------------------------------------
// Draw a single icon in the status bar
static void draw_icon_slot(display::Display& it, int cx, int cy,
                            const char* glyph, Color color) {
  it.print(cx, cy, id(font_icons_26), color,
           display::TextAlign::CENTER, glyph);
}

// -- SMART FOOTER ---------------------------------------------
// Draws updated time + battery icon (red if low) +
// thermometer icon (red if temp out of range).
// Call at the end of every page draw function.
static void draw_footer(display::Display& it) {
  auto now   = id(esptime).now();
  float bat  = id(battery_level).state;
  float temp = id(internal_temp_sensor).state;
  int   crit = id(battery_critical_pct);
  int   low  = id(battery_low_pct);
  float tmin = (float)id(display_temp_min_c);

  int x = RIGHT_X + RIGHT_W - 8;
  int y = SCREEN_H - 8;

  // -- Temperature warning (rightmost) ----------------------
  bool temp_warn = (temp < tmin || temp > 50.0f);
  if (temp_warn) {
    // Red thermometer + temp value
    char temp_str[12];
    snprintf(temp_str, sizeof(temp_str), "%.0f°", temp);
    it.print(x, y, id(font_mono_small), COLOR_RED,
             display::TextAlign::BOTTOM_RIGHT, temp_str);
    x -= 36;
    it.print(x, y, id(font_icons_22), COLOR_RED,
             display::TextAlign::BOTTOM_RIGHT, "\U000F050F"); // mdi:thermometer
    x -= 4;
  }

  // -- Battery icon + % -------------------------------------
  Color bat_color = COLOR_BLACK;
  if (bat <= crit)     bat_color = COLOR_RED;
  else if (bat <= low) bat_color = COLOR_YELLOW;

  char bat_str[8];
  snprintf(bat_str, sizeof(bat_str), "%.0f%%", bat);
  it.print(x, y, id(font_mono_small), bat_color,
           display::TextAlign::BOTTOM_RIGHT, bat_str);
  x -= 28;
  it.print(x, y, id(font_icons_22), bat_color,
           display::TextAlign::BOTTOM_RIGHT,
           battery_icon(bat, id(ha_battery_status).state));
  x -= 8;

  // -- Separator dot ----------------------------------------
  it.print(x, y, id(font_mono_small), COLOR_BLACK,
           display::TextAlign::BOTTOM_RIGHT, "·");
  x -= 8;

  // -- Updated time -----------------------------------------
  char time_str[16];
  snprintf(time_str, sizeof(time_str), "updated %02d:%02d",
           now.hour, now.minute);
  it.print(x, y, id(font_mono_small), COLOR_BLACK,
           display::TextAlign::BOTTOM_RIGHT, time_str);
}

// -- PAGE 1: MAIN DASHBOARD -----------------------------------
void draw_page1(display::Display& it) {
  it.fill(COLOR_WHITE);

  // -- Vertical divider between left bar and right panel --
  it.filled_rectangle(LEFT_BAR_W - 2, 0, 2, SCREEN_H, COLOR_BLACK);

  // ════════════════════════════════════════════════════════
  // LEFT BAR
  // ════════════════════════════════════════════════════════
  int lx = 0;
  int lw = LEFT_BAR_W - 2;
  int ly = 0;

  // -- ZONE 1: WEATHER (0–144px) -------------------------
  {
    std::string cond = id(ha_weather_condition).state;
    float temp       = id(ha_temperature).state;
    float feels      = id(ha_feels_like).state;
    float humid      = id(ha_humidity).state;
    float wind       = id(ha_wind_speed).state;

    // Temperature — large left
    char temp_str[8];
    snprintf(temp_str, sizeof(temp_str), "%.0f°", temp);
    it.print(lx + 8, ly + 8, id(font_huge), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, temp_str);

    // Weather icon — large right
    it.print(lx + lw - 8, ly + 8, id(font_icons_48),
             weather_icon_color(cond),
             display::TextAlign::TOP_RIGHT, weather_icon(cond));

    // Feels like / humidity / wind — compact row
    char sub_str[48];
    snprintf(sub_str, sizeof(sub_str), "%.0f°  %.0f%%  %.0fkm", feels, humid, wind);
    it.print(lx + 8, ly + 62, id(font_small), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, sub_str);

    // -- 3-column forecast: Later / Day1 / Day2 ----------
    const char* labels[] = {
      "LATER",
      id(ha_forecast_day1_label).state.c_str(),
      id(ha_forecast_day2_label).state.c_str()
    };
    const char* conds[] = {
      id(ha_forecast_later_condition).state.c_str(),
      id(ha_forecast_day1_condition).state.c_str(),
      id(ha_forecast_day2_condition).state.c_str()
    };
    const char* temps[] = {
      id(ha_forecast_later_temp).state.c_str(),
      id(ha_forecast_day1_temp).state.c_str(),
      id(ha_forecast_day2_temp).state.c_str()
    };

    int fc_col_w = lw / 3;
    int fc_y     = ly + 82;

    for (int i = 0; i < 3; i++) {
      int cx = lx + (i * fc_col_w) + fc_col_w / 2;
      it.print(cx, fc_y,      id(font_mono_small), COLOR_GREY,
               display::TextAlign::TOP_CENTER, labels[i]);
      it.print(cx, fc_y + 14, id(font_icons_22),
               weather_icon_color(std::string(conds[i])),
               display::TextAlign::TOP_CENTER, weather_icon(std::string(conds[i])));
      it.print(cx, fc_y + 36, id(font_small), COLOR_BLACK,
               display::TextAlign::TOP_CENTER, temps[i]);
    }

    ly = 144;
  }

  // -- ZONE 2: SOLAR 2×2 GRID (145–300px) ---------------
  {
    float solar   = id(ha_solar_today).state;
    float expected = id(ha_solar_expected).state;
    float load    = id(ha_load_today).state;
    float gexport = id(ha_grid_export).state;
    float gimport = id(ha_grid_import).state;

    char val[16], sub[20];
    int cw = lw / 2;
    int ch = 76;
    int gx = lx;
    int gy = ly + 2;

    // Top-left: Generation (yellow)
    snprintf(val, sizeof(val), "%.1f", solar);
    snprintf(sub, sizeof(sub), "of %.0f exp.", expected);
    draw_solar_cell(it, gx,      gy,      cw, ch,
                    "\U000F0D60", COLOR_YELLOW, val, sub);

    // Top-right: Load (black)
    snprintf(val, sizeof(val), "%.1f", load);
    draw_solar_cell(it, gx + cw, gy,      cw, ch,
                    "\U000F140B", COLOR_BLACK, val, "kWh load");

    // Bottom-left: Grid export (green)
    snprintf(val, sizeof(val), "%.1f", gexport);
    draw_solar_cell(it, gx,      gy + ch, cw, ch,
                    "\U000F0FD3", COLOR_GREEN, val, "kWh export");

    // Bottom-right: Grid import (red)
    snprintf(val, sizeof(val), "%.1f", gimport);
    draw_solar_cell(it, gx + cw, gy + ch, cw, ch,
                    "\U000F0FD4", COLOR_RED, val, "kWh import");

    ly += ch * 2 + 4;
  }

  // -- ZONE 3: BATTERY (300–480px) ----------------------
  {
    float soc         = id(ha_battery_soc).state;
    std::string status = id(ha_battery_status).state;
    std::string eta    = id(ha_battery_eta).state;
    int crit           = id(battery_critical_pct);

    Color bar_color = battery_bar_color(soc, status, crit);

    // Header row: icon + % + status
    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%.0f%%", soc);

    it.print(lx + 8, ly + 8, id(font_icons_32), bar_color,
             display::TextAlign::TOP_LEFT, battery_icon(soc, status));
    it.print(lx + 46, ly + 8, id(font_large), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, pct_str);

    // Status + ETA — right aligned
    std::string status_disp = status;
    // Capitalise first letter
    if (!status_disp.empty()) status_disp[0] = toupper(status_disp[0]);
    it.print(lx + lw - 8, ly + 8, id(font_small), bar_color,
             display::TextAlign::TOP_RIGHT, status_disp.c_str());

    if (!eta.empty() && status != "idle" && status != "full") {
      char eta_str[20];
      bool charging = (status == "charging");
      snprintf(eta_str, sizeof(eta_str), "%s at %s",
               charging ? "Full" : "Empty", eta.c_str());
      it.print(lx + lw - 8, ly + 28, id(font_mono_small), COLOR_GREY,
               display::TextAlign::TOP_RIGHT, eta_str);
    }

    // Fill bar
    draw_fill_bar(it, lx + 8, ly + 52, lw - 16, 24, soc, bar_color);
  }

  // ════════════════════════════════════════════════════════
  // RIGHT PANEL
  // ════════════════════════════════════════════════════════
  int rx = RIGHT_X + 4;
  int rw = RIGHT_W - 8;

  // -- ICON BAR (0–44px) --------------------------------
  {
    it.filled_rectangle(RIGHT_X, ICON_BAR_H - 1, RIGHT_W, 1, COLOR_BLACK);

    std::string alarm   = id(ha_alarm_state).state;
    std::string door    = id(ha_door_state).state;
    std::string window  = id(ha_window_state).state;
    std::string plant   = id(ha_plant_status).state;
    std::string heat    = id(ha_heating_state).state;
    std::string hotw    = id(ha_hotwater_state).state;
    std::string vac1    = id(ha_vacuum1_state).state;
    std::string vac2    = id(ha_vacuum2_state).state;
    std::string mower   = id(ha_mower_state).state;

    struct IconSlot {
      const char* glyph;
      Color color;
    };

    // -- Alarm color from globals
    Color alarm_color;
    if (alarm == "triggered")
      alarm_color = idx_color(id(color_alarm_triggered));
    else if (alarm == "armed_away" || alarm == "armed_home" || alarm == "armed_night")
      alarm_color = idx_color(id(color_alarm_armed));
    else if (alarm == "arming")
      alarm_color = idx_color(id(color_alarm_arming));
    else if (alarm == "disarmed")
      alarm_color = idx_color(id(color_alarm_disarmed));
    else
      alarm_color = idx_color(id(color_alarm_inactive));

    // -- Robot color from globals
    auto robot_color = [](const std::string& s,
                          int c_inactive, int c_running,
                          int c_scheduled, int c_error) -> Color {
      if (s == "cleaning" || s == "mowing") return idx_color(c_running);
      if (s == "scheduled")                 return idx_color(c_scheduled);
      if (s == "error")                     return idx_color(c_error);
      return idx_color(c_inactive);
    };

    // -- Plant color from globals
    Color plant_color;
    if (plant == "due")      plant_color = idx_color(id(color_plant_due));
    else if (plant == "watered") plant_color = idx_color(id(color_plant_watered));
    else if (plant == "ok")  plant_color = idx_color(id(color_plant_ok));
    else                     plant_color = idx_color(id(color_plant_inactive));

    IconSlot slots[] = {
      { "\U000F0B4E", alarm_color },
      { "\U000F081B", door   == "on" ? idx_color(id(color_door_open))     : idx_color(id(color_door_inactive))    },
      { "\U000F0A15", window == "on" ? idx_color(id(color_window_open))   : idx_color(id(color_window_inactive))  },
      { "\U000F024A", plant_color },
      { "\U000F06A0", heat  == "on"  ? idx_color(id(color_heating_active)): idx_color(id(color_heating_inactive)) },
      { "\U000F0A96", hotw  == "on"  ? idx_color(id(color_hotwater_active)): idx_color(id(color_hotwater_inactive)) },
      { "\U000F10CC", robot_color(vac1, id(color_vac1_inactive), id(color_vac1_running),
                                  id(color_vac1_scheduled), id(color_vac1_error)) },
      { "\U000F10C9", robot_color(vac2, id(color_vac2_inactive), id(color_vac2_running),
                                  id(color_vac2_scheduled), id(color_vac2_error)) },
      { "\U000F18F5", robot_color(mower, id(color_mower_inactive), id(color_mower_running),
                                  id(color_mower_scheduled), id(color_mower_error)) },
    };

    int n_slots   = 9;
    int slot_w    = RIGHT_W / n_slots;
    int icon_cy   = ICON_BAR_H / 2;

    for (int i = 0; i < n_slots; i++) {
      int cx = RIGHT_X + slot_w * i + slot_w / 2;
      draw_icon_slot(it, cx, icon_cy, slots[i].glyph, slots[i].color);
    }
  }

  // -- DETAIL TEXT AREA (44–480px) ----------------------
  {
    int ty = DETAIL_Y + 10;
    int tx = rx;

    // -- MULTI-ENTITY LOCAL ALERT EVALUATOR -------------------
    // Reads from slot_state_S_E globals (populated by poll_alert_states())
    // Builds smart label: lists active entity names, e.g.
    //   "Front door, Garage open"
    //   "Back door open"
    //   "Kitchen, Living +1 open"

    // Condition evaluator
    auto eval_cond = [](const std::string& state,
                        const std::string& cond,
                        const std::string& value) -> bool {
      if (state.empty() || state == "unknown" || state == "unavailable") return false;
      if      (cond == "eq")       return state == value;
      else if (cond == "ne")       return state != value;
      else if (cond == "contains") return state.find(value) != std::string::npos;
      else if (cond == "gt") { try { return std::stof(state) > std::stof(value); } catch(...){} }
      else if (cond == "lt") { try { return std::stof(state) < std::stof(value); } catch(...){} }
      return false;
    };

    // State cache pointer matrix
    std::string* cache[9][8] = {
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

    // Slot metadata
    std::string* labels[]  = { &id(alert_label_1), &id(alert_label_2), &id(alert_label_3),
                                &id(alert_label_4), &id(alert_label_5), &id(alert_label_6),
                                &id(alert_label_7), &id(alert_label_8), &id(alert_label_9) };
    std::string* conds[]   = { &id(alert_cond_1),  &id(alert_cond_2),  &id(alert_cond_3),
                                &id(alert_cond_4),  &id(alert_cond_5),  &id(alert_cond_6),
                                &id(alert_cond_7),  &id(alert_cond_8),  &id(alert_cond_9)  };
    std::string* values[]  = { &id(alert_value_1), &id(alert_value_2), &id(alert_value_3),
                                &id(alert_value_4), &id(alert_value_5), &id(alert_value_6),
                                &id(alert_value_7), &id(alert_value_8), &id(alert_value_9) };
    int*         enableds[]= { &id(alert_enabled_1), &id(alert_enabled_2), &id(alert_enabled_3),
                                &id(alert_enabled_4), &id(alert_enabled_5), &id(alert_enabled_6),
                                &id(alert_enabled_7), &id(alert_enabled_8), &id(alert_enabled_9) };
    Color slot_colors[] = {
      idx_color(id(color_alarm_armed)),    idx_color(id(color_door_open)),
      idx_color(id(color_window_open)),    idx_color(id(color_plant_due)),
      idx_color(id(color_heating_active)), idx_color(id(color_hotwater_active)),
      idx_color(id(color_vac1_running)),   idx_color(id(color_vac2_running)),
      idx_color(id(color_mower_running)),
    };

    bool any_alert = false;
    int  shown     = 0;
    int  overflow  = 0;
    int  max_lines = 6;

    for (int s = 0; s < 9; s++) {
      if (!*enableds[s]) continue;

      // Collect active entity names for this slot
      std::vector<std::string> active_names;
      for (int e = 0; e < 8; e++) {
        if (cache[s][e]->empty()) continue;
        if (!eval_cond(*cache[s][e], *conds[s], *values[s])) continue;

        // Get friendly name from NVS
        char name_key[16];
        snprintf(name_key, sizeof(name_key), "a_n%d_%d", e+1, s+1);
        std::string name = nvs_read_str(name_key, "");
        if (name.empty()) name = *cache[s][e];  // fallback to state
        active_names.push_back(name);
      }

      if (active_names.empty()) continue;

      // Build label string
      // Suffix = slot label (e.g. "open", "active", "running")
      std::string suffix = *labels[s];
      std::string line;

      if (active_names.size() == 1) {
        // Single: "Front door open"
        line = active_names[0] + " " + suffix;
      } else if (active_names.size() == 2) {
        // Two: "Front door, Garage open"
        line = active_names[0] + ", " + active_names[1] + " " + suffix;
      } else {
        // Three+: "Front door, Back door +N open"
        char overflow_str[8];
        snprintf(overflow_str, sizeof(overflow_str), " +%d", (int)active_names.size() - 2);
        line = active_names[0] + ", " + active_names[1] + overflow_str + " " + suffix;
      }

      // Truncate to fit display width (~42 chars at font_medium)
      if (line.length() > 42) line = line.substr(0, 39) + "...";

      if (shown < max_lines) {
        it.print(tx, ty, id(font_medium), slot_colors[s],
                 display::TextAlign::TOP_LEFT, line.c_str());
        ty += 28;
        shown++;
      } else {
        overflow++;
      }
      any_alert = true;
    }

    // Overflow indicator
    if (overflow > 0) {
      char more[16];
      snprintf(more, sizeof(more), "+ %d more...", overflow);
      it.print(tx, ty, id(font_small), COLOR_BLACK,
               display::TextAlign::TOP_LEFT, more);
      ty += 22;
    }

    // All clear
    if (!any_alert) {
      it.print(tx, ty, id(font_medium), COLOR_GREEN,
               display::TextAlign::TOP_LEFT, "\U000F0E1E  All clear");
      ty += 28;
    }

    // -- CALENDAR LINES ---------------------------------------
    ty += 6;
    const char* cal_lines[] = {
      id(ha_cal_line_1).state.c_str(),
      id(ha_cal_line_2).state.c_str(),
      id(ha_cal_line_3).state.c_str(),
      id(ha_cal_line_4).state.c_str(),
    };

    for (int i = 0; i < 4; i++) {
      if (strlen(cal_lines[i]) > 0 && strcmp(cal_lines[i], "unknown") != 0) {
        it.print(tx, ty, id(font_medium), COLOR_BLUE,
                 display::TextAlign::TOP_LEFT, cal_lines[i]);
        ty += 26;
      }
    }

    // Footer
    draw_footer(it);
  }
}

// -- PAGE 2: SECONDARY SCREEN ---------------------------------
void draw_page2(display::Display& it) {
  it.fill(COLOR_WHITE);
  it.filled_rectangle(LEFT_BAR_W - 2, 0, 2, SCREEN_H, COLOR_BLACK);

  // ════════════════════════════════════════════════════════
  // LEFT BAR — ROOM TEMPERATURES
  // ════════════════════════════════════════════════════════
  int lx = 0;
  int lw = LEFT_BAR_W - 2;
  int ly = 8;

  struct RoomRow {
    const char*   icon;
    const char*   name;
    sensor::Sensor* temp;
    sensor::Sensor* humid;
  };

  RoomRow upstairs[] = {
    { "\U000F02D8", "Main Bed", id(ha_temp_main_bed),  id(ha_humid_main_bed)  },
    { "\U000F02D8", "Guest",    id(ha_temp_guest),      id(ha_humid_guest)     },
    { "\U000F0E42", "Bathroom", id(ha_temp_bathroom),   id(ha_humid_bathroom)  },
    { "\U000F0311", "Office",   id(ha_temp_office),     id(ha_humid_office)    },
  };

  RoomRow downstairs[] = {
    { "\U000F04B9", "Living",   id(ha_temp_living),     id(ha_humid_living)    },
    { "\U000F0041", "Kitchen",  id(ha_temp_kitchen),    id(ha_humid_kitchen)   },
  };

  auto draw_room = [&](const RoomRow& r) {
    // Icon
    it.print(lx + 8, ly, id(font_icons_26), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, r.icon);
    // Name
    it.print(lx + 36, ly + 4, id(font_small), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, r.name);
    // Temp — right
    char t[8];
    snprintf(t, sizeof(t), "%.0f°", r.temp->state);
    it.print(lx + lw - 8, ly, id(font_medium), COLOR_BLACK,
             display::TextAlign::TOP_RIGHT, t);
    // Humidity — small below temp
    char h[8];
    snprintf(h, sizeof(h), "%.0f%%", r.humid->state);
    it.print(lx + lw - 8, ly + 22, id(font_mono_small), COLOR_BLUE,
             display::TextAlign::TOP_RIGHT, h);
    it.filled_rectangle(lx + 4, ly + 34, lw - 8, 1, COLOR_GREY);
    ly += 36;
  };

  // "Upstairs" label
  it.print(lx + 8, ly, id(font_mono_small), COLOR_GREY,
           display::TextAlign::TOP_LEFT, "UPSTAIRS");
  ly += 14;
  for (auto& r : upstairs) draw_room(r);

  it.filled_rectangle(lx, ly, lw, 2, COLOR_BLACK);
  ly += 6;

  // "Downstairs" label
  it.print(lx + 8, ly, id(font_mono_small), COLOR_GREY,
           display::TextAlign::TOP_LEFT, "DOWNSTAIRS");
  ly += 14;
  for (auto& r : downstairs) draw_room(r);

  // ════════════════════════════════════════════════════════
  // RIGHT PANEL
  // ════════════════════════════════════════════════════════
  int rx   = RIGHT_X + 10;
  int ry   = 10;
  int rw   = RIGHT_W - 20;

  // -- MOTION SENSORS -----------------------------------
  it.print(rx, ry, id(font_mono_small), COLOR_GREY,
           display::TextAlign::TOP_LEFT, "MOTION");
  ry += 16;

  struct MotionSlot {
    text_sensor::TextSensor* name;
    text_sensor::TextSensor* time;
    text_sensor::TextSensor* recent;  // "on"/"off"
  };

  MotionSlot motions[] = {
    { id(ha_motion1_name), id(ha_motion1_time), id(ha_motion1_recent) },
    { id(ha_motion2_name), id(ha_motion2_time), id(ha_motion2_recent) },
    { id(ha_motion3_name), id(ha_motion3_time), id(ha_motion3_recent) },
    { id(ha_motion4_name), id(ha_motion4_time), id(ha_motion4_recent) },
    { id(ha_motion5_name), id(ha_motion5_time), id(ha_motion5_recent) },
    { id(ha_motion6_name), id(ha_motion6_time), id(ha_motion6_recent) },
  };

  int motion_col_w = rw / 3;
  int motion_row = 0;
  for (int i = 0; i < 6; i++) {
    if (strcmp(motions[i].name->state.c_str(), "unknown") == 0) continue;
    bool recent  = (motions[i].recent->state == "on");
    Color mcol   = recent ? COLOR_GREEN : COLOR_GREY;
    int mx       = rx + (i % 3) * motion_col_w;
    int my       = ry + (i / 3) * 44;

    it.print(mx, my, id(font_icons_26), mcol,
             display::TextAlign::TOP_LEFT, "\U000F1A79");
    it.print(mx + 28, my + 2, id(font_small), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, motions[i].name->state.c_str());
    it.print(mx + 28, my + 18, id(font_mono_small), mcol,
             display::TextAlign::TOP_LEFT, motions[i].time->state.c_str());
  }
  ry += 96;

  it.filled_rectangle(rx, ry, rw, 1, COLOR_BLACK);
  ry += 8;

  // -- MEDIA --------------------------------------------
  it.print(rx, ry, id(font_mono_small), COLOR_GREY,
           display::TextAlign::TOP_LEFT, "NOW PLAYING");
  ry += 16;

  struct MediaSlot {
    const char* icon;
    text_sensor::TextSensor* title;
    text_sensor::TextSensor* room;
  };

  MediaSlot media[] = {
    { "\U000F03E8", id(ha_media1_title), id(ha_media1_room) },
    { "\U000F057F", id(ha_media2_title), id(ha_media2_room) },
  };

  for (auto& m : media) {
    if (strcmp(m.title->state.c_str(), "unknown") == 0 ||
        strcmp(m.title->state.c_str(), "")         == 0) continue;
    it.print(rx,      ry,      id(font_icons_26), COLOR_BLUE,
             display::TextAlign::TOP_LEFT, m.icon);
    it.print(rx + 30, ry,      id(font_medium), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, m.title->state.c_str());
    it.print(rx + 30, ry + 22, id(font_mono_small), COLOR_GREY,
             display::TextAlign::TOP_LEFT, m.room->state.c_str());
    ry += 48;
  }

  it.filled_rectangle(rx, ry, rw, 1, COLOR_BLACK);
  ry += 8;

  // -- TRANSPORT ----------------------------------------
  it.print(rx, ry, id(font_mono_small), COLOR_GREY,
           display::TextAlign::TOP_LEFT, "NEXT DEPARTURES");
  ry += 16;

  struct TransportSlot {
    const char* icon;
    const char* name;
    text_sensor::TextSensor* dep1;
    text_sensor::TextSensor* dep2;
    Color color;
  };

  TransportSlot transports[] = {
    { "\U000F020B", "42 · City Centre",      id(ha_transport1_dep1), id(ha_transport1_dep2), COLOR_GREEN },
    { "\U000F04C1", "Overground · Waterloo", id(ha_transport2_dep1), id(ha_transport2_dep2), COLOR_BLUE  },
    { "\U000F020B", "7 · Airport",           id(ha_transport3_dep1), id(ha_transport3_dep2), COLOR_GREEN },
  };

  for (auto& t : transports) {
    it.print(rx,      ry, id(font_icons_26), t.color,
             display::TextAlign::TOP_LEFT, t.icon);
    it.print(rx + 30, ry, id(font_medium), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, t.name);
    // Dep times right-aligned pair
    char deps[24];
    snprintf(deps, sizeof(deps), "%s  %s",
             t.dep1->state.c_str(), t.dep2->state.c_str());
    it.print(rx + rw, ry + 2, id(font_medium), COLOR_BLACK,
             display::TextAlign::TOP_RIGHT, deps);
    it.filled_rectangle(rx, ry + 30, rw, 1, COLOR_GREY);
    ry += 34;
  }

  // Footer
  draw_footer(it);
}

// -- PAGE 3: CONFIG MODE ---------------------------------------
void draw_page3(display::Display& it) {
  it.fill(COLOR_WHITE);

  // -- Detect AP mode --------------------------------------
  // If WiFi client is not connected, device is in AP/captive
  // portal mode. Show WiFi join QR instead of config URL QR.
  bool ap_mode = !wifi::global_wifi_component->is_connected();

  // -- Title -----------------------------------------------
  it.print(SCREEN_W / 2, 18, id(font_large), COLOR_BLACK,
           display::TextAlign::TOP_CENTER,
           ap_mode ? "WIFI SETUP" : "CONFIG MODE");

  // -- Mode icon -------------------------------------------
  it.print(SCREEN_W / 2 - 160, 14, id(font_icons_32),
           ap_mode ? COLOR_BLUE : COLOR_YELLOW,
           display::TextAlign::TOP_CENTER,
           ap_mode ? "\U000F05A9" :   // mdi:wifi-plus
                     "\U000F0493");    // mdi:cog-outline

  // -- Subtitle --------------------------------------------
  if (ap_mode) {
    it.print(SCREEN_W / 2, 54, id(font_small), COLOR_BLACK,
             display::TextAlign::TOP_CENTER,
             "Scan to connect your phone, then open the config page");
  } else {
    auto now = id(esptime).now();
    int timeout = id(config_timeout_seconds);
    int close_h = (now.hour * 3600 + now.minute * 60 + now.second + timeout) / 3600 % 24;
    int close_m = ((now.hour * 3600 + now.minute * 60 + now.second + timeout) % 3600) / 60;
    char close_str[40];
    snprintf(close_str, sizeof(close_str),
             "Auto-closes at %02d:%02d  (resets on activity)", close_h, close_m);
    it.print(SCREEN_W / 2, 54, id(font_small), COLOR_BLACK,
             display::TextAlign::TOP_CENTER, close_str);
  }

  // -- QR code (left-centre) --------------------------------
  // AP mode: encode WiFi join string so phone connects on scan
  // Normal:  encode config URL http://<device-ip>
  std::string ip = id(wifi_ip_address).state;
  std::string ap_ssid = "kitchen-panel-setup";
  std::string ap_pass = id(ap_password_global).state;  // read from NVS global

  std::string qr_content;
  std::string qr_label;
  if (ap_mode) {
    // WiFi QR format: WIFI:T:WPA;S:<ssid>;P:<pass>;;
    qr_content = "WIFI:T:WPA;S:" + ap_ssid + ";P:" + ap_pass + ";;";
    qr_label   = ap_ssid;
  } else {
    qr_content = "http://" + ip;
    qr_label   = ip;
  }

  draw_qr_placeholder(it, 200, 240, 168, qr_label);

  // -- Instructions (right of QR) ---------------------------
  int ix = 420;
  int iy = 90;

  if (ap_mode) {
    it.print(ix, iy,       id(font_medium), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, "1. Scan QR to join WiFi");
    it.print(ix, iy + 30,  id(font_medium), COLOR_BLUE,
             display::TextAlign::TOP_LEFT, ap_ssid.c_str());
    it.print(ix, iy + 64,  id(font_medium), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, "2. Open browser:");
    it.print(ix, iy + 92,  id(font_medium), COLOR_BLUE,
             display::TextAlign::TOP_LEFT, "192.168.4.1");
    it.print(ix, iy + 124, id(font_small), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, "3. Enter your WiFi + HA details");
    it.print(ix, iy + 148, id(font_small), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, "Device will reboot and connect");
  } else {
    it.print(ix, iy,       id(font_medium), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, "Open in browser:");
    char url_str[32];
    snprintf(url_str, sizeof(url_str), "http://%s", ip.c_str());
    it.print(ix, iy + 28,  id(font_medium), COLOR_BLUE,
             display::TextAlign::TOP_LEFT, url_str);
    it.print(ix, iy + 64,  id(font_medium), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, "Or scan the QR code");
    it.print(ix, iy + 100, id(font_small), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, "Sleep disabled on this screen");
    it.print(ix, iy + 120, id(font_small), COLOR_BLACK,
             display::TextAlign::TOP_LEFT, "Long press middle to exit");
  }

  // -- Status dump footer -----------------------------------
  it.filled_rectangle(0, SCREEN_H - 46, SCREEN_W, 1, COLOR_BLACK);

  auto now = id(esptime).now();
  char status1[80];
  if (ap_mode) {
    snprintf(status1, sizeof(status1),
             "AP mode: %s   FW: %s   Uptime: %lus",
             ap_ssid.c_str(),
             ESPHOME_VERSION,
             (unsigned long)(millis() / 1000));
  } else {
    snprintf(status1, sizeof(status1),
             "IP: %s   WiFi: %.0f dBm   FW: %s   Uptime: %lus   API: %s",
             ip.c_str(),
             id(wifi_signal_sensor).state,
             ESPHOME_VERSION,
             (unsigned long)(millis() / 1000),
             id(api_is_connected)() ? "connected" : "offline");
  }

  char status2[80];
  snprintf(status2, sizeof(status2),
           "Device bat: %.0f%%   Int temp: %.1f°C   Int humid: %.0f%%   Last refresh: %02d:%02d",
           id(battery_level).state,
           id(internal_temp_sensor).state,
           id(internal_humidity_sensor).state,
           now.hour, now.minute);

  it.print(8, SCREEN_H - 42, id(font_mono_small), COLOR_GREY,
           display::TextAlign::TOP_LEFT, status1);
  it.print(8, SCREEN_H - 26, id(font_mono_small), COLOR_GREY,
           display::TextAlign::TOP_LEFT, status2);
}

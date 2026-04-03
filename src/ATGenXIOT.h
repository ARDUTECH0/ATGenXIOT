/**
 * @file    ATGenXIOT.h
 * @brief   ATGenX IoT platform – single-include entry point
 * @version 2.1.0
 *
 * Include this one header in your sketch; it pulls everything in.
 *
 * @code
 *   #include <ATGenX.h>
 *
 *   ATGenX_Hub          hub("user123");
 *
 *   // ── Outputs ───────────────────────────────────────────────────────────
 *   ATGenX_Device       relay1(5,  "relay1");
 *   ATGenX_Device       relay2(18, "relay2");
 *
 *   // ── Sensors ───────────────────────────────────────────────────────────
 *   ATGenX_DHT          dht(4,  "dht1");
 *   ATGenX_PIR          pir(13, "pir1");
 *   ATGenX_LDR          ldr(34, "ldr1");
 *   ATGenX_Ultrasonic   us(12, 14, "us1");
 *
 *   void setup() {
 *       hub.begin("SSID", "password", "mqttUser", "mqttPass");
 *
 *       hub.attach(relay1);
 *       hub.attach(relay2);
 *
 *       hub.attachSensor(dht);
 *       hub.attachSensor(pir);
 *       hub.attachSensor(ldr);
 *       hub.attachSensor(us);
 *
 *       dht.begin();
 *       pir.begin();
 *       ldr.begin();
 *       us.begin();
 *   }
 *
 *   void loop() {
 *       hub.loop();   // everything runs from here
 *   }
 * @endcode
 */

#pragma once

// ── Core ──────────────────────────────────────────────────────────────────
#include "ATGenX_Hub.h"

// ── Output types ──────────────────────────────────────────────────────────
#include "ATGenX_Device.h"

// ── Sensor base + all built-in sensor types ───────────────────────────────
#include "ATGenX_Sensor.h"
#include "ATGenX_DHT.h"
#include "ATGenX_PIR.h"
#include "ATGenX_LDR.h"
#include "ATGenX_Ultrasonic.h"
#include "ATGenX_SoundSensor.h"

// ── Discovery ─────────────────────────────────────────────────────────────
#include "ATGenX_Discovery.h"
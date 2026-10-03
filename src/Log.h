#pragma once
#include <Arduino.h>
#include "config.h"

#define LOG(...)                 \
  do {                           \
    if (LOG_ENABLED) {           \
      Serial.printf(__VA_ARGS__); \
      Serial.println();          \
    }                            \
  } while (0)

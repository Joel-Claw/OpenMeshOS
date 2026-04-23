// OpenMeshOS — Log.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2

#pragma once

#include <Arduino.h>

#define OMS_LOG(tag, fmt, ...) Serial.printf("[OMS] %s: " fmt "\n", tag, ##__VA_ARGS__)
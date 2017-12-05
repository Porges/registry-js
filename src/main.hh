#pragma once

// Windows.h strict mode
#define STRICT
#define UNICODE

#include "nan.h"

#include <Windows.h>

v8::Local<v8::Array> EnumerateValues(HKEY hCurrentKey, v8::Isolate *isolate);

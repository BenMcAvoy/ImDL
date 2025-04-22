#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#define IMGUI_DEFINE_MATH_OPERATORS

#include <array>
#include <regex>
#include <vector>
#include <memory>
#include <cstring>
#include <cassert>
#include <iostream>
#include <stdint.h>
#include <functional>

#include <d3d11.h>
#include <dwmapi.h>
#include <Windows.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cpr/cpr.h>

#include "font/IconsLucide.h"
#include "font/IconsLucide.h_lucide.ttf.h"

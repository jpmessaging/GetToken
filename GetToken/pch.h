﻿#pragma once
#define WIN32_MEAN_AND_LEAN
#define NOMINMAX
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Security.Credentials.h>
#include <winrt/Windows.Security.Authentication.Web.h>
#include <winrt/Windows.Security.Authentication.Web.Core.h>
#include <winrt/Windows.Security.Authentication.Web.Provider.h>

#include <WebAuthenticationCoreManagerInterop.h>

// ABI headers (not used currently)
//#include <windows.foundation.h>
//#include <windows.security.authentication.web.core.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <numeric>
#include <optional>
#include <print>
#include <string>
#include <system_error>
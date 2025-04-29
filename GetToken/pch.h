#pragma once

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

#define SECURITY_WIN32
#include <security.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <ostream>
#include <print>
#include <ranges>
#include <regex>
#include <string_view>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

#include "popl.hpp"
﻿#include "pch.h"

#include "console.h"
#include "option.h"
#include "util.h"
#include "wam.h"

using namespace Windows::Foundation;
using namespace Windows::Security::Credentials;
using namespace Windows::Security::Authentication::Web::Core;

// Forward declarations
IAsyncOperation<int> MainAsync(const Option& option, const HWND hwnd);
WebTokenRequest GetWebTokenRequest(const WebAccountProvider& provider, WebTokenRequestPromptType promptType, const Option& option);
IAsyncOperation<WebTokenRequestResult> InvokeRequestTokenAsync(WebTokenRequest& request, HWND hwnd);
HWND CreateAnchorWindow();
void PrintWebTokenResponse(const WebTokenResponse& response);
void PrintProviderError(const WebTokenRequestResult& result);
void PrintBanner();

namespace ConsoleFormat
{
    constexpr auto Error = { Console::Format::ForegroundRed, Console::Format::Bright };
    constexpr auto Warning = { Console::Format::ForegroundYellow, Console::Format::Bright };
    constexpr auto Verbose = { Console::Format::ForegroundCyan };
}

int main(int argc, char** argv)
{
    std::ios_base::sync_with_stdio(false);
    Console::EnableVirtualTerminal();

    PrintBanner();

    auto option = Option{};

    try 
    {
        option.Parse(argc, argv);

    }
    catch (const std::exception& e)
    {
        Console::WriteLine("Failed to parse the input options. Please check the avaialble options with -h or -? switch", ConsoleFormat::Error);
        return 1;
    }

    if (option.Help())
    {
        option.PrintHelp();
        return 0;
    }

    if (option.UnknownOptions().size()) 
    {
        Console::WriteLine("Unknown options are found:", ConsoleFormat::Error);

        for (const auto& opt : option.UnknownOptions())
        {
            std::println("{}", opt);
        }

        Console::WriteLine("\nPlease check the avaialble options with --help (-h or -?)", ConsoleFormat::Error);
        return 0;
    }

    winrt::init_apartment();

    // RequestTokenAsync() needs to run on a UI thread
    // Note: I could simply use the console window:
    //   auto hwnd = GetAncestor(GetConsoleWindow(), GA_ROOTOWNER);
    // However, use a new invisible window for more control.
    auto hwnd = CreateAnchorWindow();

    // This anchor window does not have to be visible for WAM to show its window.
    // However, for the WAM window to appear in front, this one needs to be shown.
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    // Start the main async task.
    // When the task completes, destroy the window to break from the message loop
    auto task = MainAsync(option, hwnd);
    task.Completed([hwnd](auto&& async, AsyncStatus status) { SendMessage(hwnd, WM_DESTROY, 0, 0); });

    // Run the message loop till the async task completes.
    auto msg = MSG{};
    auto bRet = BOOL{};

    while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return task.GetResults();
}

IAsyncOperation<int> MainAsync(const Option& option, const HWND hwnd)
{
    const auto& provider = co_await WebAuthenticationCoreManager::FindAccountProviderAsync(WAM::ProviderId::MICROSOFT, WAM::Authority::ORGANIZATION);

    if (!provider)
    {
        Console::WriteLine(std::format(R"(FindAccountProviderAsync failed to find Provider "{}")", winrt::to_string(WAM::ProviderId::MICROSOFT)), ConsoleFormat::Error);
        //std::println(std::cerr, R"(FindAccountProviderAsync failed to find Provider "{}")", winrt::to_string(WAM::ProviderId::MICROSOFT));
        co_return 1;
    }

    Console::WriteLine("Provider:");
    Console::WriteLine(std::format("  ID: {}", winrt::to_string(provider.Id())));
    Console::WriteLine(std::format(R"(  DisplayName: "{}")", winrt::to_string(provider.DisplayName())));
    Console::WriteLine("");

    /*std::println("Provider Id: {}, DisplayName: {}", winrt::to_string(provider.Id()), winrt::to_string(provider.DisplayName()));
    std::println("");*/

    /*
    * Find Web Accounts
    */
    const auto clientId = option.ClientId().value_or(WAM::ClientId::MSOFFICE);

    const auto& findResults = co_await WebAuthenticationCoreManager::FindAllAccountsAsync(provider, clientId);
    auto accountsStatus = findResults.Status();

    if (accountsStatus != FindAllWebAccountsStatus::Success)
    {
        Console::WriteLine(std::format("FindAllAccountsAsync failed with {}", Util::to_string(accountsStatus)), ConsoleFormat::Error);
        //std::println(std::cerr, "FindAllAccountsAsync failed with {}", Util::to_string(accountsStatus));
        co_return 1;
    }

    auto accounts = findResults.Accounts();

    if (accounts.Size() == 0)
    {
        Console::WriteLine("No accounts were found");
    }
    else
    {
        Console::WriteLine(std::format("Found {} web account(s):", accounts.Size()));
        //std::println("Found {} account(s)", accounts.Size());
    }

    // Print account properties
    for (const auto& account : accounts)
    {
        std::println("");
        std::println("  ID: {}", winrt::to_string(account.Id()));
        std::println("  State: {}", Util::to_string(account.State()));
        std::println("  Properties:");

        for (const auto& [key, value] : account.Properties())
        {
            std::println("  [{},{}]", winrt::to_string(key), winrt::to_string(value));
        }

        if (option.SignOut())
        {
            Console::WriteLine("Signing out from this account ... ", ConsoleFormat::Warning);
            //std::println("\n  Signing out from this account ... ");
            account.SignOutAsync().get();
        }
    }

    std::println("");

    /*
    * Request a token
    */
    try
    {
        Console::WriteLine("Invoking WebAuthenticationCoreManager::GetTokenSilentlyAsync ...\n", ConsoleFormat::Verbose);
        //std::println("Invoking WebAuthenticationCoreManager::GetTokenSilentlyAsync ...");

        auto request = GetWebTokenRequest(provider, WebTokenRequestPromptType::Default, option);

        const auto& requestResult = co_await WebAuthenticationCoreManager::GetTokenSilentlyAsync(request);
        const auto requestStatus = requestResult.ResponseStatus();

        Console::WriteLine(std::format("GetTokenSilentlyAsync returned {}\n", Util::to_string(requestStatus)));
        //std::println(std::cerr, "GetTokenSilentlyAsync returned {}", Util::to_string(requestStatus));

        if (requestStatus == WebTokenRequestStatus::Success)
        {
            PrintWebTokenResponse(requestResult.ResponseData().GetAt(0));
        }
        else
        {
            PrintProviderError(requestResult);
        }
    }
    catch (const winrt::hresult_error& e)
    {
        // https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/error-handling
        Console::WriteLine(std::format("GetTokenSilentlyAsync failed with an exception. code:{:#x}, message:{}", static_cast<std::uint32_t>(e.code().value), winrt::to_string(e.message())), ConsoleFormat::Error);
        //std::println(std::cerr, "GetTokenSilentlyAsync failed with an exception. code:{:#x}, message:{}", static_cast<std::uint32_t>(e.code().value), winrt::to_string(e.message()));
    }

    std::println("");

    try
    {
        Console::WriteLine("Invoking WebAuthenticationCoreManager::RequestTokenAsync() ...\n", ConsoleFormat::Verbose);
        //std::println("Invoking WebAuthenticationCoreManager::RequestTokenAsync() ...");

        // Use ForceAuthentication here to show UI regardless of auth state.
        auto request = GetWebTokenRequest(provider, WebTokenRequestPromptType::ForceAuthentication, option);

        const auto& requestResult = co_await InvokeRequestTokenAsync(request, hwnd);
        auto requestStatus = requestResult.ResponseStatus();

        Console::WriteLine(std::format("RequestTokenAsync returned {}\n", Util::to_string(requestStatus)));
        //std::println("RequestTokenAsync returned {}", Util::to_string(requestStatus));

        if (requestStatus == WebTokenRequestStatus::Success)
        {
            PrintWebTokenResponse(requestResult.ResponseData().GetAt(0));
        }
        else if (requestStatus == WebTokenRequestStatus::UserCancel)
        {
            Console::WriteLine("User canceled the request", ConsoleFormat::Warning);
        }
        else
        {
            PrintProviderError(requestResult);
        }
    }
    catch (const winrt::hresult_error& e)
    {
        Console::WriteLine(std::format("RequestTokenAsync failed with an exception. code:{:#x}, message:{}", static_cast<std::uint32_t>(e.code().value), winrt::to_string(e.message())), ConsoleFormat::Error);
        // std::println(std::cerr, "RequestTokenAsync failed with an exception. code:{:#x}, message:{}", static_cast<std::uint32_t>(e.code().value), winrt::to_string(e.message()));
    }
}


WebTokenRequest GetWebTokenRequest(const WebAccountProvider& provider, const WebTokenRequestPromptType promptType, const Option& option)
{
    // If scopes & client IDs are provided, use them.
    const auto& scopes = option.Scopes().value_or(WAM::Scopes::DEFAULT_SCOPES);
    const auto& clientId = option.ClientId().value_or(WAM::ClientId::MSOFFICE);

    auto request = WebTokenRequest{ provider, scopes, clientId,promptType };

    // Add request properties
    for (const auto& [key, value] : option.Properties())
    {
        request.Properties().Insert(key, value);
    }

    return request;
}

IAsyncOperation<WebTokenRequestResult> InvokeRequestTokenAsync(WebTokenRequest& request, const HWND hwnd)
{
    // Invoke RequestTokenAsync() via IWebAuthenticationCoreManagerInterop::RequestTokenForWindowAsync()
    // https://devblogs.microsoft.com/oldnewthing/20210805-00/?p=105520
    auto interop = winrt::get_activation_factory<WebAuthenticationCoreManager, IWebAuthenticationCoreManagerInterop>();
    auto requestInspectable = static_cast<::IInspectable*>(winrt::get_abi(request));

    auto getTokenTask = winrt::capture<IAsyncOperation<WebTokenRequestResult>>(
        interop,
        &IWebAuthenticationCoreManagerInterop::RequestTokenForWindowAsync,
        hwnd,
        requestInspectable);

    co_return co_await getTokenTask;
}

void PrintWebTokenResponse(const WebTokenResponse& response)
{
    std::println("  WebAccount Id:{}", winrt::to_string(response.WebAccount().Id()));
    std::println("  WebTokenResponse Properties:\n");

    for (const auto& [key, value] : response.Properties())
    {
        std::println("  [{},{}]", winrt::to_string(key), winrt::to_string(value));
    }

    auto provError = response.ProviderError();

    if (provError)
    {
        Console::WriteLine(std::format("ErrorCode: {:#x}, ErrorMessage: {}", static_cast<std::uint32_t>(provError.ErrorCode()), winrt::to_string(provError.ErrorMessage())), ConsoleFormat::Error);
        //std::println("ErrorCode: {:#x}, ErrorMessage: {}", static_cast<std::uint32_t>(provError.ErrorCode()), winrt::to_string(provError.ErrorMessage()));
    }
}

void PrintProviderError(const WebTokenRequestResult& result)
{
    // ResponseError might be null (e.g. when status is WebTokenRequestStatus::UserCancel)
    if (const auto& error = result.ResponseError())
    {
        Console::WriteLine(std::format("ErrorCode: {:#x}, ErrorMessage: {}", static_cast<std::uint32_t>(error.ErrorCode()), winrt::to_string(error.ErrorMessage())), ConsoleFormat::Error);
        //std::println("ErrorCode: {:#x}, ErrorMessage: {}", static_cast<std::uint32_t>(error.ErrorCode()), winrt::to_string(error.ErrorMessage()));
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

HWND CreateAnchorWindow()
{
    WNDCLASSEX wndclass{
    .cbSize = sizeof(WNDCLASSEX),
    .style = CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc = WndProc,
    .cbClsExtra = 0,
    .cbWndExtra = 0,
    .hInstance = GetModuleHandleW(nullptr),
    .hIcon = LoadIcon(NULL, IDI_APPLICATION),
    .hCursor = LoadCursor(NULL, IDC_ARROW),
    .hbrBackground = GetSysColorBrush(COLOR_3DFACE),
    .lpszMenuName = nullptr,
    .lpszClassName = L"WndClass",
    .hIconSm = nullptr
    };

    RegisterClassExW(&wndclass);

    // Place at center of desktop
    auto rect = RECT{};
    GetClientRect(GetDesktopWindow(), &rect);
    auto x = rect.right / 2;
    auto y = rect.bottom / 2;
    auto width = 0;
    auto height = 0;

    auto hwnd = CreateWindowExW(
        0,
        wndclass.lpszClassName,
        L"Anchor Window",
        WS_POPUP, // WS_OVERLAPPED, 
        x, y, width, height,
        nullptr,
        nullptr,
        GetModuleHandle(NULL),
        nullptr);

    if (hwnd == NULL)
    {
        throw std::system_error{ static_cast<int>(GetLastError()), std::system_category(), "CreateWindowExW filed" };
    }

    return hwnd;
}

/// <summary>
/// Print file name and vesrion
/// </summary>
void PrintBanner()
{
    //Get this executable file name & version
    static const auto exeName = []() {
        auto exePath = std::string(MAX_PATH, 0);
        auto cch = GetModuleFileNameA(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
        exePath.resize(cch);
        return exePath.substr(exePath.rfind('\\') + 1);
    }();

    static const auto version = []() {
        auto exePath = std::array<wchar_t, MAX_PATH>{};
        GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
        auto version = Util::GetFileVersion(exePath.data());
        return Util::to_string(version.value_or(L""));
    }();

    //std::println("{} (version {})", exeName, version);
    Console::WriteLine(std::format("{} (version {})", exeName, version), ConsoleFormat::Verbose);
    Console::WriteLine("");
}

// refs:
// https://stackoverflow.com/questions/59994731/how-can-i-programmatically-force-an-iasyncoperation-into-the-error-state
// https://stackoverflow.com/questions/42812444/gettokensilentlyasync-doesnt-execute
// https://github.com/CommunityToolkit/Graph-Controls/blob/main/CommunityToolkit.Authentication.Uwp/WindowsProvider.cs
// https://github.com/microsoft/Windows-universal-samples/tree/main/Samples/WebAccountManagement/cpp
// https://kennykerrca.wordpress.com/2018/03/08/cppwinrt-handling-async-completion/

// AppProperties
// https://github.com/MicrosoftDocs/windows-dev-docs/blob/docs/uwp/security/web-account-manager.md
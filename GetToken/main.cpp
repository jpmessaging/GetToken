#include "pch.h"

#include "console.h"
#include "option.h"
#include "trace.h"
#include "util.h"
#include "wam.h"

using namespace Windows::Foundation;
using namespace Windows::Security::Credentials;
using namespace Windows::Security::Authentication::Web::Core;

using namespace Diagnostics;

// Forward declarations
auto ParseOption(int argc, char** argv) -> std::expected<Option, std::string>;
void EnableTrace(const Option& option);
auto MainAsync(const Option& option, const HWND hwnd) -> IAsyncOperation<int>;
auto GetWebTokenRequest(const WebAccountProvider& provider, WebTokenRequestPromptType promptType, const Option& option) -> WebTokenRequest;
auto InvokeRequestTokenAsync(WebTokenRequest& request, HWND hwnd) -> IAsyncOperation<WebTokenRequestResult>;
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
    winrt::init_apartment();
    
    Console::EnableVirtualTerminal();

    auto option = ParseOption(argc, argv);

    if (not option)
    {
        Console::WriteLine(ConsoleFormat::Error, "{}", option.error());
        return 1;
    }

    const auto exePath = Util::GetModulePath(nullptr);
    const auto exeName = exePath.stem();
    auto version = Util::GetFileVersion(exePath.c_str());
    Console::WriteLine(ConsoleFormat::Verbose, L"{} (version {})\n", exeName.c_str(), version.value_or(L""));

    if (option->Help())
    {
        option->PrintHelp();
        return 0;
    }

    // Always enable tracing
    EnableTrace(*option);
    Trace::Write(L"{} (version {} {})", exeName.c_str(), version.value_or(L""), GetCurrentProcessId());

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
    auto task = MainAsync(*option, hwnd);
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

    if (not provider)
    {
        Console::WriteLine(ConsoleFormat::Error, LR"(FindAccountProviderAsync failed to find Provider "{}")", WAM::ProviderId::MICROSOFT);
        Trace::Write(LR"(FindAccountProviderAsync failed to find Provider "{}")", WAM::ProviderId::MICROSOFT);

        co_return 1;
    }

    Console::WriteLine("Provider:");
    Console::WriteLine(L"  ID: {}", provider.Id());
    Console::WriteLine(LR"(  DisplayName: "{}")", provider.DisplayName());
    Console::WriteLine("");

    Trace::Write(LR"(Provider: {}; DisplayName: "{}")", provider.Id(), provider.DisplayName());

    /*
    * Find Web Accounts
    */
    const auto clientId = option.ClientId().value_or(WAM::ClientId::MSOFFICE);

    const auto& findResults = co_await WebAuthenticationCoreManager::FindAllAccountsAsync(provider, clientId);
    auto accountsStatus = findResults.Status();

    if (accountsStatus != FindAllWebAccountsStatus::Success)
    {
        Console::WriteLine(ConsoleFormat::Error, "FindAllAccountsAsync failed with {}", Util::to_string(accountsStatus));
        co_return 1;
    }

    auto accounts = findResults.Accounts();
    Trace::Write("Found {} web account(s)", accounts.Size());

    if (accounts.Size() == 0)
    {
        Console::Write("No accounts were found");
    }
    else
    {
        Console::WriteLine("Found {} web account(s):", accounts.Size());
    }

    // Print account properties
    for (const auto& account : accounts)
    {
        Trace::Write(L"Account Id: {}, State: {}", account.Id(), Util::to_wstring(account.State()));

        Console::WriteLine("");
        Console::WriteLine(L"  ID: {}", account.Id());
        Console::WriteLine("  State: {}", Util::to_string(account.State()));
        Console::WriteLine("  Properties:");

        for (const auto& [key, value] : account.Properties())
        {
            Console::WriteLine(L"  [{},{}]", key, value);
            Trace::Write(L"  [{},{}]", key, value);
        }

        if (option.SignOut())
        {
            Console::WriteLine(ConsoleFormat::Warning, "\n  Signing out from this account ... ");            
            Trace::Write(L"Signing out of account {}", account.Id());
            account.SignOutAsync().get();
        }
    }

    Console::WriteLine("");

    /*
    * Request a token
    */
    try
    {
        Console::WriteLine(ConsoleFormat::Verbose, "Invoking WebAuthenticationCoreManager::GetTokenSilentlyAsync ...\n");
        Trace::Write("Invoking WebAuthenticationCoreManager::GetTokenSilentlyAsync ...");

        auto request = GetWebTokenRequest(provider, WebTokenRequestPromptType::Default, option);

        const auto& requestResult = co_await WebAuthenticationCoreManager::GetTokenSilentlyAsync(request);
        const auto requestStatus = requestResult.ResponseStatus();

        Console::WriteLine("GetTokenSilentlyAsync returned {}", Util::to_string(requestStatus));
        Trace::Write("GetTokenSilentlyAsync returned {}", Util::to_string(requestStatus));

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
        Console::WriteLine(ConsoleFormat::Error, L"GetTokenSilentlyAsync failed with an exception. code:{:#x}, message:{}", static_cast<std::uint32_t>(e.code().value), e.message());
        Trace::Write(L"GetTokenSilentlyAsync failed with an exception. code:{:#x}, message:{}", static_cast<std::uint32_t>(e.code().value), e.message());
    }

    Console::WriteLine("");

    try
    {
        Console::WriteLine(ConsoleFormat::Verbose, "Invoking WebAuthenticationCoreManager::RequestTokenAsync ...\n");
        Trace::Write("Invoking WebAuthenticationCoreManager::RequestTokenAsync ...");

        // Use ForceAuthentication here to show UI regardless of auth state.
        auto request = GetWebTokenRequest(provider, WebTokenRequestPromptType::ForceAuthentication, option);

        const auto& requestResult = co_await InvokeRequestTokenAsync(request, hwnd);
        auto requestStatus = requestResult.ResponseStatus();

        Console::WriteLine("RequestTokenAsync returned {}\n", Util::to_string(requestStatus));
        Trace::Write(L"RequestTokenAsync returned {}\n", Util::to_wstring(requestStatus));

        if (requestStatus == WebTokenRequestStatus::Success)
        {
            PrintWebTokenResponse(requestResult.ResponseData().GetAt(0));
        }
        else if (requestStatus == WebTokenRequestStatus::UserCancel)
        {
            Console::WriteLine(ConsoleFormat::Warning, "User canceled the request");
            Trace::Write("User canceled the request");
        }
        else
        {
            PrintProviderError(requestResult);
        }
    }
    catch (const winrt::hresult_error& e)
    {
        Console::WriteLine(ConsoleFormat::Error, L"RequestTokenAsync failed with an exception. code:{:#x}, message:{}", static_cast<std::uint32_t>(e.code().value), e.message());
        Trace::Write(L"RequestTokenAsync failed with an exception. code:{:#x}, message:{}", static_cast<std::uint32_t>(e.code().value), e.message());
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
    Console::WriteLine(L"  WebAccount Id:{}", response.WebAccount().Id());
    Console::WriteLine("  WebTokenResponse Properties:\n");

    Trace::Write(L"WebAccount Id:{}", response.WebAccount().Id());
    Trace::Write("WebTokenResponse Properties:");

    for (const auto& [key, value] : response.Properties())
    {
        Console::WriteLine(L"  [{},{}]", key, value);
        Trace::Write(L"  [{},{}]", key, value);
    }

    auto provError = response.ProviderError();

    if (provError)
    {
        Console::WriteLine(ConsoleFormat::Error, L"ErrorCode: {:#x}, ErrorMessage: {}", static_cast<std::uint32_t>(provError.ErrorCode()), provError.ErrorMessage());
        Trace::Write(L"ErrorCode: {:#x}, ErrorMessage: {}", static_cast<std::uint32_t>(provError.ErrorCode()), provError.ErrorMessage());
    }
}

void PrintProviderError(const WebTokenRequestResult& result)
{
    // ResponseError might be null (e.g. when status is WebTokenRequestStatus::UserCancel)
    if (const auto& error = result.ResponseError())
    {
        Console::WriteLine(ConsoleFormat::Error, L"ErrorCode: {:#x}, ErrorMessage: {}", static_cast<std::uint32_t>(error.ErrorCode()), error.ErrorMessage());
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
        const auto exePath = Util::GetModulePath(nullptr).string();
        return exePath.substr(exePath.rfind('\\') + 1);
    }();

    static const auto version = []() {
        const auto exePath = Util::GetModulePath(nullptr).wstring();
        auto version = Util::GetFileVersion(exePath.data());
        return Util::to_string(version.value_or(L""));
    }();

    Console::WriteLine(ConsoleFormat::Verbose, "{} (version {})\n", exeName, version);
    Trace::Write("{} (version {}), PID: {}\n", exeName, version, GetCurrentProcessId());
}

void EnableTrace(const Option& option)
{
    auto exePath = Util::GetModulePath(nullptr);
    auto path = option.TracePath().has_value() ? std::filesystem::path{ option.TracePath().value() } : exePath.parent_path();

    try 
    {
        // If the path does not exist, create it (This may throw)
        if (not std::filesystem::exists(path))
        {
            std::filesystem::create_directory(path);
        }

        // Use this executable's name & current time as file name.
        using namespace std::chrono;

        auto fileName = exePath.stem();
        fileName += "_";
        fileName += std::format(L"{0:%Y%m%d}_{0:%H%M%S}", time_point_cast<seconds>(system_clock::now()));
        fileName.replace_extension(L"log");

         path /= fileName;

        Trace::Enable(path.wstring());
    }
    catch (const std::exception& e)
    {
        Console::WriteLine(ConsoleFormat::Error, "Failed to create a trace folder {}", path.string());
    }
}

/// <summary>
/// Parse input options. On failure, it returns a error message.
/// </summary>
/// <param name="argc">arg count</param>
/// <param name="argv">array of arg strings</param>
/// <returns></returns>
std::expected<Option, std::string> ParseOption(int argc, char** argv)
{
    auto option = Option{};

    try
    {
        option.Parse(argc, argv);
    }
    catch (const std::exception& e)
    {
        return std::unexpected{ "Failed to parse the input options. Please check the avaialble options with -h or -? switch" };
    }

    // If unknown options are given, display them and exit
    if (option.UnknownOptions().size())
    {
        auto error = std::string{ "Unknown options are found:" };

        for (const auto& opt : option.UnknownOptions())
        {
            error.append(opt).append("\n");
        }

        error.append("\nPlease check the avaialble options with --help (-h or -?)");
        return std::unexpected{ error };
    }

    return option;
}

// refs:
// https://stackoverflow.com/questions/59994731/how-can-i-programmatically-force-an-iasyncoperation-into-the-error-state
// https://stackoverflow.com/questions/42812444/gettokensilentlyasync-doesnt-execute
// https://github.com/CommunityToolkit/Graph-Controls/blob/main/CommunityToolkit.Authentication.Uwp/WindowsProvider.cs
// https://github.com/microsoft/Windows-universal-samples/tree/main/Samples/WebAccountManagement/cpp
// https://kennykerrca.wordpress.com/2018/03/08/cppwinrt-handling-async-completion/

// AppProperties
// https://github.com/MicrosoftDocs/windows-dev-docs/blob/docs/uwp/security/web-account-manager.md
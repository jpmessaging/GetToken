#include "pch.h"

#include "console.h"
#include "option.h"
#include "trace.h"
#include "util.h"
#include "wam.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Security::Credentials;
using namespace Windows::Security::Authentication::Web::Core;
using namespace Diagnostics;

// Forward declarations
auto ParseOption(int argc, char** argv) noexcept -> std::expected<Option, std::string>;
void EnableTrace(const Option& option) noexcept;
auto MainAsync(const Option& option, const HWND hwnd) -> IAsyncOperation<int>;
auto GetWebTokenRequest(const WebAccountProvider& provider, WebTokenRequestPromptType promptType, const Option& option) -> WebTokenRequest;
auto InvokeRequestTokenAsync(const WebTokenRequest& request, HWND hwnd) -> IAsyncOperation<WebTokenRequestResult>;
HWND CreateAnchorWindow();
void PrintWebAccount(const WebAccount& account) noexcept;
void PrintWebTokenResponse(const WebTokenResponse& response, bool showToken = false) noexcept;
void PrintProviderError(const WebProviderError& error) noexcept;

// Logger writes to both console & trace file
namespace Logger
{
    template <typename... Args>
    void WriteLine(const std::format_string<Args...> format, Args&&... args) noexcept;

    template <typename... Args>
    void WriteLine(const std::initializer_list<Console::Format>& consoleFormat, const std::format_string<Args...> format, Args&&... args) noexcept;

    template <typename... Args>
    void WriteLine(const std::wformat_string<Args...> format, Args&&... args) noexcept;

    template <typename... Args>
    void WriteLine(const std::initializer_list<Console::Format>& consoleFormat, const std::wformat_string<Args...> format, Args&&... args) noexcept;
}

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
    auto coUninit = Util::DtorAction{ []() { winrt::uninit_apartment(); } };

    Console::EnableVirtualTerminal();
    auto ConsoleDtor = Util::DtorAction{ []() { Console::Uninit(); } };

    auto option = ParseOption(argc, argv);

    if (not option)
    {
        Console::WriteLine(ConsoleFormat::Error, "{}", option.error());
        return EXIT_FAILURE;
    }

    Console::WriteLine(ConsoleFormat::Verbose, "{}", option->GetVersion());

    if (option->Help())
    {
        Console::WriteLine("{}", option->GetHelp());
        return EXIT_SUCCESS;
    }

    if (option->Version())
    {
        return EXIT_SUCCESS;
    }

    if (option->EnableTrace())
    {
        EnableTrace(*option);
    }

    Trace::Write("{}", option->GetVersion());
    Trace::Write("CommandLine: {}", GetCommandLineA());

    auto currentUser = Util::GetCurrentUserName();
    Trace::Write("Current User: {}", currentUser.has_value() ? currentUser.value() : currentUser.error());

    if (option->Wait())
    {
        Console::Write(ConsoleFormat::Warning, "Hit enter to continue...");
        std::cin.ignore();
    }

    // RequestTokenAsync() needs to run on a UI thread
    // Note: I could simply use the console window:
    //   auto hwnd = GetAncestor(GetConsoleWindow(), GA_ROOTOWNER);
    // However, use a separate invisible window for more control.
    auto hwnd = CreateAnchorWindow();

    // Start the main async task.
    // When the task completes, destroy the window to exit message loop
    auto task = MainAsync(*option, hwnd);
    task.Completed([hwnd](auto&& /*async*/, AsyncStatus /*status*/) { SendMessage(hwnd, WM_DESTROY, 0, 0); });

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
        Logger::WriteLine(ConsoleFormat::Error, LR"(FindAccountProviderAsync failed to find Provider "{}")", WAM::ProviderId::MICROSOFT);
        co_return EXIT_FAILURE;
    }

    Logger::WriteLine("Provider:");
    Logger::WriteLine(L"  ID: {}", provider.Id());
    Logger::WriteLine(R"(  DisplayName: "{}")", Util::to_string(provider.DisplayName()));
    Logger::WriteLine("");

    /*
    * Find Web Accounts
    */
    auto webAccounts = std::vector<WebAccount>{};

    const auto clientId = option.ClientId().value_or(WAM::ClientId::MSOFFICE);

    const auto& findResults = co_await WebAuthenticationCoreManager::FindAllAccountsAsync(provider, clientId);
    auto accountsStatus = findResults.Status();

    if (accountsStatus == FindAllWebAccountsStatus::Success)
    {
        auto accounts = findResults.Accounts();

        if (accounts.Size() == 0)
        {
            Logger::WriteLine("No accounts were found");
        }
        else
        {
            Logger::WriteLine("Found {} web account(s):", accounts.Size());
        }

        // Print account properties
        for (const auto& account : accounts)
        {
            PrintWebAccount(account);
            Logger::WriteLine("");

            if (option.SignOut())
            {
                Logger::WriteLine(ConsoleFormat::Warning, "  Signing out from this account ... ");
                account.SignOutAsync().get();
            }
            else 
            {
                webAccounts.push_back(account);
            }
        }
    }
    else
    {
        Logger::WriteLine(ConsoleFormat::Error, "FindAllAccountsAsync failed with {}", accountsStatus);
        PrintProviderError(findResults.ProviderError());
    }

    if (option.ShowAccountsOnly())
    {
        Trace::Write("Exiting because of ShowAccountsOnly option");
        co_return EXIT_SUCCESS;
    }

    /*
    * Request a token
    */

    // Invoke GetTokenSilentlyAsync for each Web Account. Even if there's no account, still try GetTokenSilentlyAsync without an account.
    {
        auto done = bool{};
        auto i = int{};
        const auto request = GetWebTokenRequest(provider, WebTokenRequestPromptType::Default, option);
        
        while (not done)
        {
            try
            {
                auto pResult = std::optional<WebTokenRequestResult>{};

                if (webAccounts.size() == 0)
                {
                    Logger::WriteLine(ConsoleFormat::Verbose, "Invoking WebAuthenticationCoreManager::GetTokenSilentlyAsync ...");
                    pResult = co_await WebAuthenticationCoreManager::GetTokenSilentlyAsync(request);
                    done = true;
                }
                else
                {
                    const auto& webAccount = webAccounts[i];
                    Logger::WriteLine(ConsoleFormat::Verbose, L"Invoking WebAuthenticationCoreManager::GetTokenSilentlyAsync for Web Account {} ...", webAccount.Id());
                    pResult = co_await WebAuthenticationCoreManager::GetTokenSilentlyAsync(request, webAccount);
                    done = (++i == webAccounts.size());
                }

                Logger::WriteLine("GetTokenSilentlyAsync's ResponseStatus: {}", pResult->ResponseStatus());

                if (pResult->ResponseStatus() == WebTokenRequestStatus::Success)
                {
                    PrintWebTokenResponse(pResult->ResponseData().GetAt(0), option.ShowToken());
                }
                else
                {
                    PrintProviderError(pResult->ResponseError());
                }
            }
            catch (const winrt::hresult_error& e)
            {
                // https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/error-handling
                Logger::WriteLine(ConsoleFormat::Error, L"GetTokenSilentlyAsync failed with an exception. code:{:#x}; message:{}", static_cast<std::uint32_t>(e.code()), e.message());
            }

            Console::WriteLine("");
        }
    }

    // Invoke RequestTokenAsync (via IWebAuthenticationCoreManagerInterop::RequestTokenForWindowAsync)
    try
    {
        Logger::WriteLine(ConsoleFormat::Verbose, "Invoking WebAuthenticationCoreManager::RequestTokenAsync ...");

        // Use ForceAuthentication here to show UI regardless of auth state.
        const auto request = GetWebTokenRequest(provider, WebTokenRequestPromptType::ForceAuthentication, option);

        const auto& requestResult = co_await InvokeRequestTokenAsync(request, hwnd);
        auto requestStatus = requestResult.ResponseStatus();

        Logger::WriteLine("RequestTokenAsync's ResponseStatus: {}", requestStatus);

        if (requestStatus == WebTokenRequestStatus::Success)
        {
            PrintWebTokenResponse(requestResult.ResponseData().GetAt(0), option.ShowToken());
        }
        else
        {
            if (requestStatus == WebTokenRequestStatus::UserCancel)
            {
                Logger::WriteLine(ConsoleFormat::Warning, "User canceled the request");
            }

            PrintProviderError(requestResult.ResponseError());
        }
    }
    catch (const winrt::hresult_error& e)
    {
        Logger::WriteLine(ConsoleFormat::Error, L"RequestTokenAsync failed with an exception. code:{:#x}; message:{}", static_cast<std::uint32_t>(e.code()), e.message());
    }
}


WebTokenRequest GetWebTokenRequest(const WebAccountProvider& provider, const WebTokenRequestPromptType promptType, const Option& option)
{
    // If scopes & client IDs are provided, use them.
    const auto clientId = option.ClientId().value_or(WAM::ClientId::MSOFFICE);
    const auto scopes = option.Scopes().value_or(WAM::Scopes::DEFAULT_SCOPES);
    
    auto request = WebTokenRequest{ provider, scopes, clientId, promptType };

    // Add request properties
    for (const auto& [key, value] : option.Properties())
    {
        request.Properties().Insert(key, value);
    }

    // Log request properties
    Trace::Write("WebTokenRequest:");
    Trace::Write(L"  clientId: {}", request.ClientId());
    Trace::Write(L"  Scope: '{}'", request.Scope());
    Trace::Write("  PromptType: {}", request.PromptType());
    Trace::Write(L"  CorrelationId: {}", request.CorrelationId());

    for (auto&& [key, val] : request.Properties())
    {
        Trace::Write(L"  Property: {}={}", key, val);
    }

    return request;
}

IAsyncOperation<WebTokenRequestResult> InvokeRequestTokenAsync(const WebTokenRequest& request, const HWND hwnd)
{
    // Note that WebAuthenticationCoreManager.RequestTokenAsync() cannot be directly called from Win32.
    // https://learn.microsoft.com/en-us/uwp/api/windows.security.authentication.web.core.webauthenticationcoremanager.requesttokenasync
    // Invoke RequestTokenAsync() via IWebAuthenticationCoreManagerInterop::RequestTokenForWindowAsync()
    // https://devblogs.microsoft.com/oldnewthing/20210805-00/?p=105520

    auto pIWebAuthenticationCoreManagerInterop = winrt::get_activation_factory<WebAuthenticationCoreManager, IWebAuthenticationCoreManagerInterop>();
    auto requestInspectable = static_cast<::IInspectable*>(winrt::get_abi(request));

   /*
    auto asyncOp = IAsyncOperation<WebTokenRequestResult>{};

    auto hr = pIWebAuthenticationCoreManagerInterop->RequestTokenForWindowAsync(
        hwnd,
        requestInspectable,
        winrt::guid_of<IAsyncOperation<WebTokenRequestResult>>(),
        reinterpret_cast<void**>(&asyncOp)
    );

    winrt::check_hresult(hr);
    return asyncOp;
   */

    auto asyncOp = winrt::capture<IAsyncOperation<WebTokenRequestResult>>(
        pIWebAuthenticationCoreManagerInterop,
        &IWebAuthenticationCoreManagerInterop::RequestTokenForWindowAsync,
        hwnd,
        requestInspectable);

    return asyncOp;
}

void PrintWebTokenResponse(const WebTokenResponse& response, bool showToken) noexcept
{
    Logger::WriteLine(L"  WebAccount Id:{}", response.WebAccount().Id());

    if (showToken) 
    {
        Console::WriteLine(L"  Token: {}\n", response.Token());
    }

    Logger::WriteLine("  WebTokenResponse Properties:\n");

    for (const auto& [key, value] : response.Properties())
    {
        Logger::WriteLine(L"  [{},{}]", key, value);
    }

    // Print response's error if any
    PrintProviderError(response.ProviderError());
}

void PrintProviderError(const WebProviderError& error) noexcept
{
    // ResponseError might be null (e.g. when status is WebTokenRequestStatus::UserCancel)
    if (error)
    {
        Logger::WriteLine(ConsoleFormat::Error, L"ErrorCode: {:#x}; ErrorMessage: {}", static_cast<std::uint32_t>(error.ErrorCode()), error.ErrorMessage());
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

    auto hwndConsole = GetAncestor(GetConsoleWindow(), GA_ROOTOWNER);

    auto hwnd = CreateWindowExW(
        0,
        wndclass.lpszClassName,
        L"Anchor Window",
        WS_POPUP,
        x, y, width, height,
        hwndConsole, // hWndParent
        nullptr,
        GetModuleHandle(NULL),
        nullptr);

    if (hwnd == NULL)
    {
        throw std::system_error{ static_cast<int>(GetLastError()), std::system_category(), "CreateWindowExW failed" };
    }

    return hwnd;
}

void PrintWebAccount(const WebAccount& account) noexcept
{
    Logger::WriteLine(L"  ID: {}", account.Id());
    Logger::WriteLine("  State: {}", account.State());
    Logger::WriteLine("  Properties:");

    for (const auto& [key, value] : account.Properties())
    {
        Logger::WriteLine(L"  [{},{}]", key, value);
    }
}

void EnableTrace(const Option& option) noexcept
{
    auto exePath = Util::GetModulePath(nullptr);
    auto path = option.TracePath().has_value() ? option.TracePath().value() : exePath.parent_path();

    try
    {
        // If the path does not exist, create it (this may throw)
        if (not std::filesystem::exists(path))
        {
            std::filesystem::create_directory(path);
        }

        // Use this executable's name & current time as file name.
        using namespace std::chrono;

        auto fileName = exePath.stem();
        fileName += "_";
        fileName += std::format(L"{0:%F}T{0:%H%M%S}Z", time_point_cast<seconds>(utc_clock::now()));
        fileName.replace_extension(L"log");

        path /= fileName;

        Trace::Enable(path);
    }
    catch (const std::exception& e)
    {
        Console::WriteLine(ConsoleFormat::Error, "Failed to create a trace folder {}. {}", path.string(), e.what());
    }
}

/// <summary>
/// Parse input options. On failure, it returns an error message.
/// </summary>
/// <param name="argc">arg count</param>
/// <param name="argv">array of arg strings</param>
/// <returns></returns>
std::expected<Option, std::string> ParseOption(int argc, char** argv) noexcept
{
    try
    {
        auto option = Option{ argc, argv };

        if (option.UnknownOptions().size())
        {
            auto error = std::string{ "Unknown options are found:\n" };

            for (const auto& opt : option.UnknownOptions())
            {
                error.append(opt).append("\n");
            }

            error.append("\nPlease check the avaialble options with --help (-h or -?)");
            return std::unexpected{ error };
        }

        return option;
    }
    catch (...)
    {
        return std::unexpected{ "Failed to parse the input options. Please check the avaialble options with -h or -? switch" };
    }
}

namespace Logger
{
    /// <summary>
    /// Write message to both console & log file
    /// </summary>
    template <typename... Args>
    void WriteLine(const std::format_string<Args...> format, Args&&... args) noexcept
    {
        auto message = std::format(format, std::forward<Args>(args)...);
        Trace::Write("{}", message);
        Console::WriteLine("{}", message);
    }

    template <typename... Args>
    void WriteLine(const std::initializer_list<Console::Format>& consoleFormat, const std::format_string<Args...> format, Args&&... args) noexcept
    {
        auto message = std::format(format, std::forward<Args>(args)...);
        Trace::Write("{}", message);
        Console::WriteLine(consoleFormat, "{}", message);
    }

    template <typename... Args>
    void WriteLine(const std::wformat_string<Args...> format, Args&&... args) noexcept
    {
        auto message = std::format(format, std::forward<Args>(args)...);
        Trace::Write(L"{}", message);
        Console::WriteLine(L"{}", message);
    }

    template <typename... Args>
    void WriteLine(const std::initializer_list<Console::Format>& consoleFormat, const std::wformat_string<Args...> format, Args&&... args) noexcept
    {
        auto message = std::format(format, std::forward<Args>(args)...);
        Trace::Write(L"{}", message);
        Console::WriteLine(consoleFormat, L"{}", message);
    }
}
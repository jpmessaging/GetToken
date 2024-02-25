#include "pch.h"

#include "wam.h"
#include "util.h"
#include "option.h"

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

int main(int argc, char** argv)
{
    std::ios_base::sync_with_stdio(false);

    const auto option = Option{argc, argv};

    if (option.Help())
    {
        option.PrintHelp();
        return 0;
    }

    if (option.UnknownOptions().size()) 
    {
        std::println("Unknown options are found:");

        for (const auto& opt : option.UnknownOptions())
        {
            std::println("{}", opt);
        }
        
        std::println("\nPlease check the avaialble options with --help (-h or -?)");
        return 0;
    }

    winrt::init_apartment();

    // RequestTokenAsync() needs to run on a UI thread
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
        std::println(std::cerr, R"(FindAccountProviderAsync failed to find Provider "{}")", winrt::to_string(WAM::ProviderId::MICROSOFT));
        co_return 1;
    }

    std::println("Provider Id: {}, DisplayName: {}", winrt::to_string(provider.Id()), winrt::to_string(provider.DisplayName()));
    std::println("");

    /*
    * Find Web Accounts
    */
    const auto clientId = option.ClientId().value_or(WAM::ClientId::MSOFFICE);

    const auto& findResults = co_await WebAuthenticationCoreManager::FindAllAccountsAsync(provider, clientId);
    auto accountsStatus = findResults.Status();

    if (accountsStatus != FindAllWebAccountsStatus::Success)
    {
        std::println(std::cerr, "FindAllAccountsAsync failed with {}", Util::to_string(accountsStatus));
        co_return 1;
    }

    auto accounts = findResults.Accounts();
    std::println("Found {} account(s)", accounts.Size());

    for (const auto& account : accounts)
    {
        std::println("");

        for (const auto& [key, value] : account.Properties())
        {
            std::println("  [{},{}]", winrt::to_string(key), winrt::to_string(value));
        }

        if (option.SignOut())
        {
            std::println("\n  Signing out from this account ... ");
            account.SignOutAsync().get();
        }
    }

    std::println("");

    /*
    * Request a token
    */
    try
    {
        std::println("Invoking WebAuthenticationCoreManager::GetTokenSilentlyAsync ...");

        auto request = GetWebTokenRequest(provider, WebTokenRequestPromptType::Default, option);

        const auto& requestResult = co_await WebAuthenticationCoreManager::GetTokenSilentlyAsync(request);

        const auto requestStatus = requestResult.ResponseStatus();
        std::println(std::cerr, "GetTokenSilentlyAsync returned {}", Util::to_string(requestStatus));

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
        std::println(std::cerr, "GetTokenSilentlyAsync failed with an exception. code:{:#x}, message:{}", static_cast<std::uint32_t>(e.code().value), winrt::to_string(e.message()));
    }

    std::println("");

    try
    {
        std::println("Invoking WebAuthenticationCoreManager::RequestTokenAsync() ...");

        // Use ForceAuthentication here to show UI regardless of auth state.
        auto request = GetWebTokenRequest(provider, WebTokenRequestPromptType::ForceAuthentication, option);

        const auto& requestResult = co_await InvokeRequestTokenAsync(request, hwnd);

        auto requestStatus = requestResult.ResponseStatus();
        std::println("RequestTokenAsync returned {}", Util::to_string(requestStatus));

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
        std::println(std::cerr, "RequestTokenAsync failed with an exception. code:{:#x}, message:{}", static_cast<std::uint32_t>(e.code().value), winrt::to_string(e.message()));
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
    //std::println("  WebAccount Id:{}\n  Token: {}", winrt::to_string(response.WebAccount().Id()), winrt::to_string(response.Token()));
    std::println("  WebAccount Id:{}", winrt::to_string(response.WebAccount().Id()));
    std::println("  WebTokenResponse Properties:\n");

    for (const auto& [key, value] : response.Properties())
    {
        std::println("  [{},{}]", winrt::to_string(key), winrt::to_string(value));
    }

    auto provError = response.ProviderError();

    if (provError)
    {
        std::println("ErrorCode: {:#x}, ErrorMessage: {}", static_cast<std::uint32_t>(provError.ErrorCode()), winrt::to_string(provError.ErrorMessage()));
    }
}

void PrintProviderError(const WebProviderError& providerError)
{
    std::println("ErrorCode: {:#x}, ErrorMessage: {}", static_cast<std::uint32_t>(providerError.ErrorCode()), winrt::to_string(providerError.ErrorMessage()));
}

void PrintProviderError(const WebTokenRequestResult& result)
{
    // ResponseError might be null (e.g. when status is WebTokenRequestStatus::UserCancel)
    if (auto error = result.ResponseError())
    {
        PrintProviderError(error);
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

// refs:
// https://stackoverflow.com/questions/59994731/how-can-i-programmatically-force-an-iasyncoperation-into-the-error-state
// https://stackoverflow.com/questions/42812444/gettokensilentlyasync-doesnt-execute
// https://github.com/CommunityToolkit/Graph-Controls/blob/main/CommunityToolkit.Authentication.Uwp/WindowsProvider.cs
// https://github.com/microsoft/Windows-universal-samples/tree/main/Samples/WebAccountManagement/cpp
// https://kennykerrca.wordpress.com/2018/03/08/cppwinrt-handling-async-completion/

// AppProperties
// https://github.com/MicrosoftDocs/windows-dev-docs/blob/docs/uwp/security/web-account-manager.md
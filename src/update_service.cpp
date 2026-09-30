#include "update_service.hpp"
#include "update_policy.hpp"
#include "update_hash.hpp"

#include "config.hpp"
#include "service_imports.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include "mods/svc/http.h"

#include <array>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace twilight_hd_hud {
namespace {

constexpr const char* kLatestReleaseApi =
    "https://api.github.com/repos/OTPR26/twilight-hd-hud/releases/latest";

#if defined(TARGET_OS_TV) && TARGET_OS_TV
constexpr const char* kReleaseAsset = "Twilight-HD-HUD-tvOS.dusk";
constexpr bool kCanSelfInstall = false;
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
constexpr const char* kReleaseAsset = "Twilight-HD-HUD.dusk";
constexpr bool kCanSelfInstall = false;
#else
constexpr const char* kReleaseAsset = "Twilight-HD-HUD.dusk";
constexpr bool kCanSelfInstall = true;
#endif

enum class State { Idle, Checking, CheckReady, AwaitingChoice, Downloading, DownloadReady };

struct Result {
    bool succeeded = false;
    bool updateAvailable = false;
    bool manual = false;
    bool installSucceeded = false;
    std::string currentVersion;
    std::string latestVersion;
    std::string downloadUrl;
    std::string digest;
    std::uint64_t downloadSize = 0;
};

std::atomic<State> s_state{State::Idle};
Result s_result;
HttpRequestHandle s_request = 0;

void on_check_setting_changed(ModContext*, ConfigVarHandle,
    const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value != nullptr && value->type == CONFIG_VAR_BOOL && value->bool_value) {
        request_update_check(nullptr, nullptr);
    }
}

std::filesystem::path update_target() {
    const char* dataDir = nullptr;
    if (svc_host == nullptr ||
        svc_host->data_dir(mod_ctx, &dataDir) != MOD_OK || dataDir == nullptr || *dataDir == 0) {
        return {};
    }
    const auto mods = std::filesystem::path(dataDir).parent_path().parent_path() / "mods";
    std::filesystem::path candidate;
    std::size_t count = 0;
    std::error_code ec;
    const char* knownNames[] = {"Twilight-HD-HUD.dusk", "Twilight-HD-HUD-Desktop-Android.dusk",
        "twilight_hd_hud.dusk", "Twilight-HD-HUD-iOS-tvOS.dusk", "Twilight-HD-HUD-tvOS.dusk"};
    for (const auto* name : knownNames) {
        const auto path = mods / name;
        if (std::filesystem::is_regular_file(path, ec)) { candidate = path; ++count; }
        // A missing optional legacy filename is expected; access errors fail closed.
        if (ec && ec != std::errc::no_such_file_or_directory) return {};
        ec.clear();
    }
    // Never create a second copy or replace an unrelated/import package.
    return count == 1 ? candidate : std::filesystem::path{};
}

bool valid_download(const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::file_size(path, ec) != s_result.downloadSize || ec) return false;
    std::ifstream input(path, std::ios::binary);
    char magic[4]{};
    input.read(magic, sizeof(magic));
    if (input.gcount() != 4 || magic[0] != 'P' || magic[1] != 'K' || magic[2] != 3 || magic[3] != 4)
        return false;
    input.seekg(0);
    return update_policy::sha256(input) == s_result.digest;
}

std::filesystem::path download_staging_path() {
    const char* dataDir = nullptr;
    if (svc_host == nullptr ||
        svc_host->data_dir(mod_ctx, &dataDir) != MOD_OK || dataDir == nullptr || *dataDir == 0) {
        return {};
    }
    return std::filesystem::path(dataDir) / "twilight-hd-update.dusk";
}

bool replace_download(const std::filesystem::path& downloaded) {
    if (!kCanSelfInstall) return false;
    namespace fs = std::filesystem;
    const fs::path target = update_target();
    if (target.empty() || downloaded.empty() || !valid_download(downloaded)) return false;
    const fs::path temporary = target.string() + ".download";
    const fs::path backup = target.string() + ".previous";
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    fs::remove(temporary, ec);
    std::error_code copyError;
    fs::copy_file(downloaded, temporary, fs::copy_options::overwrite_existing, copyError);
    std::error_code cleanupError;
    fs::remove(downloaded, cleanupError);
    if (copyError || !valid_download(temporary)) {
        fs::remove(temporary, ec);
        return false;
    }

    if (fs::exists(backup, ec) || ec) {
        fs::remove(temporary, ec);
        return false;
    }
    ec.clear();
    if (fs::exists(target, ec)) {
        fs::rename(target, backup, ec);
        if (ec) {
            fs::remove(temporary, ec);
            return false;
        }
    }
    ec.clear();
    fs::rename(temporary, target, ec);
    if (!ec) {
        // The backup is needed only while replacement is in progress. Once
        // the new package occupies the original path, do not leave an
        // unexplained file in the user's mods directory.
        fs::remove(backup, ec);
        return true;
    }

    fs::remove(temporary, ec);
    if (fs::exists(backup, ec)) {
        ec.clear();
        fs::rename(backup, target, ec);
    }
    return false;
}

bool http_succeeded(const HttpResult* result) {
    return result != nullptr && result->error == HTTP_ERROR_NONE &&
        result->status_code >= 200 && result->status_code < 300;
}

void finish_request(State readyState) {
    s_request = 0;
    s_state.store(readyState, std::memory_order_release);
}

void on_check_complete(ModContext*, HttpRequestHandle, const HttpResult* response, void*) {
    if (s_state.load(std::memory_order_acquire) != State::Checking) return;
    Result result = s_result;
    if (http_succeeded(response) && response->body != nullptr && response->body_size != 0) {
        const std::string json{static_cast<const char*>(response->body), response->body_size};
        update_policy::Release release;
        result.succeeded = update_policy::release(json, kReleaseAsset, release);
        if (result.succeeded) {
            result.latestVersion = release.tag;
            result.downloadUrl = release.url;
            result.digest = release.digest;
            result.downloadSize = release.size;
            result.updateAvailable = update_policy::newer(result.latestVersion, result.currentVersion);
        }
    }
    s_result = std::move(result);
    finish_request(State::CheckReady);
}

void on_download_complete(ModContext*, HttpRequestHandle, const HttpResult* response, void*) {
    if (s_state.load(std::memory_order_acquire) != State::Downloading) return;
    const std::filesystem::path downloaded = download_staging_path();
    s_result.installSucceeded = http_succeeded(response) && replace_download(downloaded);
    if (!s_result.installSucceeded && !downloaded.empty()) {
        std::error_code ec;
        std::filesystem::remove(downloaded, ec);
    }
    finish_request(State::DownloadReady);
}

void dismiss_update(ModContext*, UiDialogHandle, void*) {
    State expected = State::AwaitingChoice;
    s_state.compare_exchange_strong(expected, State::Idle);
}

void show_dialog(const char* title, const std::string& body, UiDialogVariant variant,
    const UiDialogAction* actions, std::size_t count) {
    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = title;
    desc.body_rml = body.c_str();
    desc.variant = variant;
    desc.actions = actions;
    desc.action_count = count;
    desc.on_dismiss = dismiss_update;
    if (svc_ui->dialog_push(mod_ctx, &desc, nullptr) != MOD_OK) dismiss_update(nullptr, 0, nullptr);
}

void confirm_update(ModContext*, UiDialogHandle, void*) {
    if (!kCanSelfInstall) return;
    const std::string url = s_result.downloadUrl;
    if (url.empty() || svc_http == nullptr) return;
    const auto staging = download_staging_path();
    if (staging.empty()) {
        s_result.installSucceeded = false;
        s_state.store(State::DownloadReady, std::memory_order_release);
        return;
    }
    std::error_code ec;
    std::filesystem::remove(staging, ec);

    State expected = State::AwaitingChoice;
    if (!s_state.compare_exchange_strong(expected, State::Downloading)) return;
    const std::string stagingText = staging.string();
    HttpRequestDesc request = HTTP_REQUEST_DESC_INIT;
    request.url = url.c_str();
    request.download_path = stagingText.c_str();
    request.connect_timeout_ms = 8000;
    request.idle_timeout_ms = 30000;
    request.total_timeout_ms = 120000;
    if (svc_http->request(mod_ctx, &request, on_download_complete, nullptr, &s_request) != MOD_OK) {
        s_result.installSucceeded = false;
        finish_request(State::DownloadReady);
    }
}

}  // namespace

void request_update_check(ModContext*, void* userData) {
    State expected = State::Idle;
    if (!s_state.compare_exchange_strong(expected, State::Checking)) return;
    if (svc_http == nullptr) {
        s_result = {};
        s_result.manual = userData != nullptr;
        finish_request(State::CheckReady);
        return;
    }
    const char* version = svc_host->mod_version(mod_ctx);
    s_result = {};
    s_result.manual = userData != nullptr;
    s_result.currentVersion = version != nullptr ? version : "unknown";

    const HttpHeader header{"Accept", "application/vnd.github+json"};
    HttpRequestDesc request = HTTP_REQUEST_DESC_INIT;
    request.url = kLatestReleaseApi;
    request.headers = &header;
    request.header_count = 1;
    request.connect_timeout_ms = 8000;
    request.idle_timeout_ms = 10000;
    request.total_timeout_ms = 30000;
    request.max_body_bytes = 1024 * 1024;
    if (svc_http->request(mod_ctx, &request, on_check_complete, nullptr, &s_request) != MOD_OK) {
        finish_request(State::CheckReady);
    }
}

bool update_service_busy(ModContext*, void*) {
    return s_state.load(std::memory_order_acquire) != State::Idle;
}

void initialize_update_service() {
    // HTTP is optional so offline hosts can still load the HUD.
    s_state.store(State::Idle, std::memory_order_release);
    s_request = 0;
    svc_config->subscribe(mod_ctx, check_for_updates_config_var(),
        on_check_setting_changed, nullptr, nullptr);
    if (check_for_updates_enabled()) {
        request_update_check(nullptr, nullptr);
    }
}

void update_update_service() {
    const State state = s_state.load(std::memory_order_acquire);
    if (state != State::CheckReady && state != State::DownloadReady) return;
    const Result result = s_result;
    s_state.store(State::Idle, std::memory_order_release);

    const UiDialogAction ok{
        sizeof(UiDialogAction), "OK", nullptr, nullptr, false, nullptr};
    if (state == State::DownloadReady) {
        if (result.installSucceeded) {
            show_dialog("Update Complete",
                "<b>Twilight HD</b> has been updated to <b>" + result.latestVersion +
                    "</b>.<br/><br/>Restart Dusklight to apply the update.",
                UI_DIALOG_NORMAL, &ok, 1);
        } else {
            show_dialog("Update Failed",
                "The update could not be verified or installed. Use one standard-named Twilight HD package in the mods folder, "
                "or install the release manually. Your existing mod was preserved.",
                UI_DIALOG_DANGER, &ok, 1);
        }
        return;
    }

    if (!result.succeeded) {
        if (result.manual) {
            show_dialog("Update Check Failed",
                "The GitHub release could not be checked. Verify your connection and that this Dusklight build supports HTTPS requests.",
                UI_DIALOG_WARNING, &ok, 1);
        }
    } else if (result.updateAvailable) {
        if (!kCanSelfInstall) {
            show_dialog("Mod Update Available",
                "A newer version of <b>Twilight HD</b> is available.<br/><br/>Installed: <b>v" +
                    result.currentVersion + "</b><br/>Latest: <b>" + result.latestVersion +
                    "</b><br/><br/>On iOS and tvOS, the updated mod must be bundled and signed "
                    "with Dusklight. Install an app build containing the updated mod. "
                    "This platform supports update checks only; no update has been installed.",
                UI_DIALOG_NORMAL, &ok, 1);
            return;
        }
        static UiDialogAction actions[2];
        actions[0] = {
            sizeof(UiDialogAction), "Update Now", confirm_update, nullptr, false, nullptr};
        actions[1] = {
            sizeof(UiDialogAction), "Not Now", dismiss_update, nullptr, false, nullptr};
        s_state.store(State::AwaitingChoice, std::memory_order_release);
        show_dialog("Mod Update Available",
            "A newer version of <b>Twilight HD</b> is available.<br/><br/>Installed: <b>v" +
                result.currentVersion + "</b><br/>Latest: <b>" + result.latestVersion +
                "</b><br/><br/>Download and install it now?",
            UI_DIALOG_NORMAL, actions, 2);
    } else if (result.manual) {
        show_dialog("Twilight HD Is Up to Date",
            "Installed: <b>v" + result.currentVersion + "</b><br/>Latest: <b>" +
                result.latestVersion + "</b>", UI_DIALOG_NORMAL, &ok, 1);
    }
}

void shutdown_update_service() {
    s_state.store(State::Idle, std::memory_order_release);
    if (s_request != 0 && svc_http != nullptr) {
        svc_http->cancel(mod_ctx, s_request);
    }
    s_request = 0;
}

}  // namespace twilight_hd_hud

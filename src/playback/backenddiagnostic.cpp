#include "backenddiagnostic.h"
#include "backendinfo.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mpv/client.h>
#include <utility>

namespace {
struct MpvDeleter {
    void operator()(mpv_handle *handle) const { mpv_terminate_destroy(handle); }
};
} // namespace

int checkBackend() {
    const auto handle = std::unique_ptr<mpv_handle, MpvDeleter>(mpv_create());
    if (!handle) {
        std::cerr << "mpv_create failed\n";
        return EXIT_FAILURE;
    }
    constexpr auto options =
        std::array{std::pair{"config", "no"}, std::pair{"load-scripts", "no"},
                   std::pair{"terminal", "no"}, std::pair{"vo", "null"}, std::pair{"ao", "null"}};
    for (const auto &[name, value] : options) {
        const int status = mpv_set_option_string(handle.get(), name, value);
        if (status < 0) {
            std::cerr << name << ": " << mpv_error_string(status) << '\n';
            return EXIT_FAILURE;
        }
    }
    const int status = mpv_initialize(handle.get());
    if (status < 0) {
        std::cerr << "mpv_initialize: " << mpv_error_string(status) << '\n';
        return EXIT_FAILURE;
    }
    const auto version = std::unique_ptr<char, decltype(&mpv_free)>(
        mpv_get_property_string(handle.get(), "mpv-version"), &mpv_free);
    if (!version) {
        std::cerr << "Cannot read mpv-version\n";
        return EXIT_FAILURE;
    }
    const auto api = BackendInfo::clientApiVersion();
    if (api.isEmpty()) {
        std::cerr << "Cannot determine mpv client API version\n";
        return EXIT_FAILURE;
    }
    std::cout << "Backend ready: " << version.get() << "; client API " << api.toStdString() << '\n';
    return EXIT_SUCCESS;
}

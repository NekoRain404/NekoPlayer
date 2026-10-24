#include "backendinfo.h"

#include <mpv/client.h>

QString BackendInfo::clientApiVersion() {
    const auto version = mpv_client_api_version();
    return QStringLiteral("%1.%2").arg(version >> 16U).arg(version & 0xffffU);
}

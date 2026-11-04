#include "playback/mpv/mpvcontroller_p.h"

#include <QByteArray>
#include <QList>
#include <vector>

const NetworkOptions &MpvController::networkOptions() const { return impl_->network; }

void MpvController::setNetworkOptions(NetworkOptions options) {
    auto &self = *impl_;
    options.proxy = options.proxy.trimmed();
    options.cookiesFile = options.cookiesFile.trimmed();
    QStringList headers;
    for (const auto &line : options.headers) {
        const auto trimmed = line.trimmed();
        if (!trimmed.isEmpty() && headers.size() < 32)
            headers.append(trimmed);
    }
    options.headers = headers;
    if (self.network.proxy == options.proxy && self.network.headers == options.headers &&
        self.network.cookiesFile == options.cookiesFile)
        return;
    self.network = std::move(options);
    self.applyNetworkOptions();
}

void MpvController::Impl::applyNetworkOptions() {
    if (!ready)
        return;
    report(mpv_set_property_string(client.get(), "http-proxy", network.proxy.toUtf8().constData()));
    report(mpv_set_property_string(client.get(), "cookies-file",
                                   network.cookiesFile.toUtf8().constData()));

    QList<QByteArray> storage;
    storage.reserve(network.headers.size());
    for (const auto &header : network.headers)
        storage.append(header.toUtf8());
    std::vector<mpv_node> nodes(storage.size());
    for (qsizetype index = 0; index < storage.size(); ++index) {
        nodes[static_cast<size_t>(index)].format = MPV_FORMAT_STRING;
        nodes[static_cast<size_t>(index)].u.string = storage[index].data();
    }
    mpv_node_list list{};
    list.num = static_cast<int>(nodes.size());
    list.values = nodes.empty() ? nullptr : nodes.data();
    mpv_node value{};
    value.format = MPV_FORMAT_NODE_ARRAY;
    value.u.list = &list;
    report(mpv_set_property(client.get(), "http-header-fields", MPV_FORMAT_NODE, &value));
}

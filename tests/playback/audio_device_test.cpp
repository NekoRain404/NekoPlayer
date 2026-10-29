#include "playback/mpv/audiodevice.h"

#include <QtTest>
#include <array>

namespace {
mpv_node stringNode(const char *value) {
    mpv_node node{};
    node.format = MPV_FORMAT_STRING;
    node.u.string = const_cast<char *>(value);
    return node;
}

mpv_node mapNode(mpv_node_list &map, const char **keys, mpv_node *values, int count) {
    map.num = count;
    map.keys = const_cast<char **>(keys);
    map.values = values;
    mpv_node node{};
    node.format = MPV_FORMAT_NODE_MAP;
    node.u.list = &map;
    return node;
}

mpv_node arrayNode(mpv_node_list &array, mpv_node *values, int count) {
    array.num = count;
    array.values = values;
    mpv_node node{};
    node.format = MPV_FORMAT_NODE_ARRAY;
    node.u.list = &array;
    return node;
}
} // namespace

class AudioDeviceTest final : public QObject {
    Q_OBJECT
  private slots:
    void parsesDevicesAndAddsAutomaticChoice() {
        const char *keys[] = {"name", "description"};
        auto firstValues = std::array{stringNode("alsa/default"), stringNode("Built-in")};
        auto secondValues = std::array{stringNode("pulse/main"), stringNode("Speakers")};
        mpv_node_list firstMap{};
        mpv_node_list secondMap{};
        auto entries = std::array{mapNode(firstMap, keys, firstValues.data(), 2),
                                  mapNode(secondMap, keys, secondValues.data(), 2)};
        mpv_node_list array{};

        const auto devices = parseMpvAudioDevices(arrayNode(array, entries.data(), entries.size()));

        QCOMPARE(devices.size(), 3);
        QCOMPARE(devices[0].name, QStringLiteral("auto"));
        QCOMPARE(devices[0].description, QStringLiteral("自动选择"));
        QCOMPARE(devices[1].name, QStringLiteral("alsa/default"));
        QCOMPARE(devices[1].description, QStringLiteral("Built-in"));
        QCOMPARE(devices[2].name, QStringLiteral("pulse/main"));
        QCOMPARE(devices[2].description, QStringLiteral("Speakers"));
    }

    void keepsExistingAutomaticChoiceInItsOriginalPosition() {
        const char *keys[] = {"name", "description"};
        auto values = std::array{stringNode("auto"), stringNode("Automatic")};
        mpv_node_list map{};
        auto entry = mapNode(map, keys, values.data(), 2);
        mpv_node_list array{};

        const auto devices = parseMpvAudioDevices(arrayNode(array, &entry, 1));

        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices.first().name, QStringLiteral("auto"));
        QCOMPARE(devices.first().description, QStringLiteral("Automatic"));
    }

    void ignoresInvalidNodesAndFields() {
        const char *invalidKeys[] = {"name", "description"};
        mpv_node invalidValues[] = {stringNode(""), stringNode("Empty name")};
        mpv_node_list invalidMap{};
        auto emptyName = mapNode(invalidMap, invalidKeys, invalidValues, 2);

        const char *wrongTypeKeys[] = {"name", "description"};
        mpv_node wrongTypeValues[2]{};
        wrongTypeValues[0].format = MPV_FORMAT_INT64;
        wrongTypeValues[0].u.int64 = 5;
        wrongTypeValues[1] = stringNode("No string name");
        mpv_node_list wrongTypeMap{};
        auto wrongNameType = mapNode(wrongTypeMap, wrongTypeKeys, wrongTypeValues, 2);
        mpv_node nonMap{};
        nonMap.format = MPV_FORMAT_INT64;
        mpv_node entries[] = {emptyName, wrongNameType, nonMap};
        mpv_node_list array{};

        const auto devices = parseMpvAudioDevices(arrayNode(array, entries, 3));

        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices.first().name, QStringLiteral("auto"));
    }

    void invalidTopLevelNodeReturnsAutomaticChoice() {
        mpv_node invalid{};
        invalid.format = MPV_FORMAT_STRING;
        invalid.u.string = nullptr;

        const auto devices = parseMpvAudioDevices(invalid);

        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices.first().name, QStringLiteral("auto"));
        QCOMPARE(devices.first().description, QStringLiteral("自动选择"));
    }

    void removesDuplicateNamesAndKeepsFirstDescription() {
        const char *keys[] = {"name", "description"};
        auto firstValues = std::array{stringNode("device-1"), stringNode("First")};
        auto duplicateValues = std::array{stringNode("device-1"), stringNode("Duplicate")};
        mpv_node_list firstMap{};
        mpv_node_list duplicateMap{};
        auto entries = std::array{mapNode(firstMap, keys, firstValues.data(), 2),
                                  mapNode(duplicateMap, keys, duplicateValues.data(), 2)};
        mpv_node_list array{};

        const auto devices = parseMpvAudioDevices(arrayNode(array, entries.data(), entries.size()));

        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices[1].name, QStringLiteral("device-1"));
        QCOMPARE(devices[1].description, QStringLiteral("First"));
    }
};

QTEST_GUILESS_MAIN(AudioDeviceTest)
#include "audio_device_test.moc"

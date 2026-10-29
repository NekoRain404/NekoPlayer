#include "configfile.h"

#include <QFile>
#include <QFileInfo>

void restrictConfigurationFilePermissions(const QString &filePath) {
    const auto userOnly = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
    QFile::setPermissions(filePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QFile::setPermissions(QFileInfo(filePath).absolutePath(), userOnly);
}

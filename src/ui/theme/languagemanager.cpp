#include "ui/theme/languagemanager.h"

#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

LanguageManager::LanguageManager(QCoreApplication &application)
    : QObject(&application), application_(application) {}

LanguageManager::~LanguageManager() {
    if (translator_)
        application_.removeTranslator(translator_.get());
    if (qtTranslator_)
        application_.removeTranslator(qtTranslator_.get());
}

bool LanguageManager::setLanguage(const QString &language) {
    if (language != QLatin1String("zh_CN") && language != QLatin1String("en"))
        return false;
    if (initialized_ && language == currentLanguage_)
        return true;

    std::unique_ptr<QTranslator> next;
    if (language == QLatin1String("en")) {
        next = std::make_unique<QTranslator>();
        if (!next->load(QStringLiteral(":/i18n/NekoPlayer_en.qm")))
            return false;
    }

    std::unique_ptr<QTranslator> nextQt;
    if (language == QLatin1String("zh_CN")) {
        nextQt = std::make_unique<QTranslator>();
        const auto directory = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
        if (!nextQt->load(QLocale(QStringLiteral("zh_CN")), QStringLiteral("qtbase"),
                          QStringLiteral("_"), directory))
            nextQt.reset();
    }

    if (translator_)
        application_.removeTranslator(translator_.get());
    if (qtTranslator_)
        application_.removeTranslator(qtTranslator_.get());
    translator_ = std::move(next);
    qtTranslator_ = std::move(nextQt);
    if (translator_)
        application_.installTranslator(translator_.get());
    if (qtTranslator_)
        application_.installTranslator(qtTranslator_.get());
    currentLanguage_ = language;
    initialized_ = true;
    return true;
}

QString LanguageManager::currentLanguage() const { return currentLanguage_; }

#include "languagemanager.h"

#include <QTranslator>

LanguageManager::LanguageManager(QCoreApplication &application)
    : QObject(&application), application_(application) {}

LanguageManager::~LanguageManager() {
    if (translator_)
        application_.removeTranslator(translator_.get());
}

bool LanguageManager::setLanguage(const QString &language) {
    if (language != QLatin1String("zh_CN") && language != QLatin1String("en"))
        return false;
    if (language == currentLanguage_)
        return true;

    std::unique_ptr<QTranslator> next;
    if (language == QLatin1String("en")) {
        next = std::make_unique<QTranslator>();
        if (!next->load(QStringLiteral(":/i18n/NekoPlayer_en.qm")))
            return false;
    }

    if (translator_)
        application_.removeTranslator(translator_.get());
    translator_ = std::move(next);
    if (translator_)
        application_.installTranslator(translator_.get());
    currentLanguage_ = language;
    return true;
}

QString LanguageManager::currentLanguage() const { return currentLanguage_; }

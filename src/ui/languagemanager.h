#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <memory>

class QTranslator;

class LanguageManager final : public QObject {
  public:
    explicit LanguageManager(QCoreApplication &application);
    ~LanguageManager() override;
    [[nodiscard]] bool setLanguage(const QString &language);
    [[nodiscard]] QString currentLanguage() const;

  private:
    QCoreApplication &application_;
    std::unique_ptr<QTranslator> translator_;
    QString currentLanguage_ = QStringLiteral("zh_CN");
};

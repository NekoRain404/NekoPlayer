#include "playback/mpvcontroller.h"
#include "ui/preferencesdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QtTest>

class PreferencesAppearanceTest final : public QObject {
    Q_OBJECT
  private slots:
    void appearanceApplyCommitsBaselineAndDefaultsStayOnAppearance() {
        using Appearance = AppearancePreferences;
        MpvController player(true, true);
        PreferencesDialog dialog(
            player, nullptr, {}, true, 5,
            {Appearance::Theme::Light, Appearance::Accent::Amber, false, 55, 72});
        auto *theme = dialog.findChild<QComboBox *>(QStringLiteral("appearanceThemeChoice"));
        auto *accent = dialog.findChild<QComboBox *>(QStringLiteral("appearanceAccentChoice"));
        auto *glass = dialog.findChild<QCheckBox *>(QStringLiteral("appearanceGlassChoice"));
        auto *opacity = dialog.findChild<QSlider *>(QStringLiteral("appearanceGlassOpacityChoice"));
        auto *blur = dialog.findChild<QSlider *>(QStringLiteral("appearanceGlassBlurChoice"));
        QVERIFY(theme && accent && glass && opacity && blur);
        QCOMPARE(theme->currentData().toInt(), 1);
        QCOMPARE(accent->currentData().toInt(), 4);
        QVERIFY(!glass->isChecked());
        QCOMPARE(opacity->value(), 55);
        QCOMPARE(blur->value(), 72);
        QVERIFY(!opacity->isEnabled());
        QVERIFY(!blur->isEnabled());
        for (int i = 0; i < 3; ++i)
            QCOMPARE(theme->itemData(i).toInt(), i);
        for (int i = 0; i < 5; ++i)
            QCOMPARE(accent->itemData(i).toInt(), i);
        QSignalSpy changed(&dialog, &PreferencesDialog::appearancePreferencesChanged);
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        dialog.selectCategory(PreferencesDialog::Category::Appearance);
        QVERIFY(!buttons->button(QDialogButtonBox::RestoreDefaults)->isHidden());
        buttons->button(QDialogButtonBox::RestoreDefaults)->click();
        QCOMPARE(theme->currentData().toInt(), 0);
        QCOMPARE(accent->currentData().toInt(), 0);
        QVERIFY(glass->isChecked());
        QCOMPARE(opacity->value(), glassOpacityDefault);
        QCOMPARE(blur->value(), glassBlurDefault);
        QVERIFY(opacity->isEnabled());
        QVERIFY(blur->isEnabled());
        QVERIFY(changed.isEmpty());
        QVERIFY(buttons->button(QDialogButtonBox::Apply)->isEnabled());
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(changed.count(), 1);
        const auto applied = qvariant_cast<Appearance::Settings>(changed.at(0).at(0));
        QCOMPARE(applied.theme, Appearance::Theme::Dark);
        QCOMPARE(applied.accent, Appearance::Accent::Ocean);
        QVERIFY(applied.glassEnabled);
        QCOMPARE(applied.glassOpacity, glassOpacityDefault);
        QCOMPARE(applied.glassBlur, glassBlurDefault);
        const auto committedStyle = qApp->styleSheet();
        const auto committedPalette = qApp->palette();
        QVERIFY(!buttons->button(QDialogButtonBox::Apply)->isEnabled());
        accent->setCurrentIndex(2);
        dialog.reject();
        QCOMPARE(qApp->styleSheet(), committedStyle);
        QCOMPARE(qApp->palette(), committedPalette);
        QCOMPARE(changed.count(), 1);
        dialog.selectCategory(PreferencesDialog::Category::Playback);
        QVERIFY(buttons->button(QDialogButtonBox::RestoreDefaults)->isHidden());
    }

    void appearanceControlsPreviewAndCancel() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        auto *theme = dialog.findChild<QComboBox *>(QStringLiteral("appearanceThemeChoice"));
        auto *accent = dialog.findChild<QComboBox *>(QStringLiteral("appearanceAccentChoice"));
        auto *glass = dialog.findChild<QCheckBox *>(QStringLiteral("appearanceGlassChoice"));
        QVERIFY(theme && accent && glass);
        QCOMPARE(theme->count(), 3);
        QCOMPARE(accent->count(), 5);
        QCOMPARE(theme->accessibleName(), QStringLiteral("基础主题"));
        QCOMPARE(accent->accessibleName(), QStringLiteral("强调色"));
        QCOMPARE(glass->accessibleName(), QStringLiteral("启用玻璃质感"));
        QVERIFY(glass->isChecked());
        const auto originalPalette = qApp->palette();
        const auto originalStyle = qApp->styleSheet();
        theme->setCurrentIndex(1);
        accent->setCurrentIndex(3);
        glass->setChecked(false);
        QVERIFY(qApp->palette() != originalPalette);
        QVERIFY(qApp->styleSheet() != originalStyle);
        dialog.reject();
        QCOMPARE(qApp->palette(), originalPalette);
        QCOMPARE(qApp->styleSheet(), originalStyle);
    }
};

QTEST_MAIN(PreferencesAppearanceTest)
#include "preferences_appearance_test.moc"

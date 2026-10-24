#include "ui/appearancepreferences.h"
#include "ui/appearancestylesheet.h"
#include "ui/appearancetokens.h"

#include <QApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QSettings>
#include <QStyle>
#include <QTemporaryDir>
#include <QTranslator>
#include <QtTest>

#include <algorithm>
#include <cmath>

class AppearancePreferencesTest final : public QObject {
    Q_OBJECT
  private slots:
    void missingAppearanceSettingsUseDefaults() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings store(directory.filePath(QStringLiteral("appearance.ini")), QSettings::IniFormat);
        const auto settings = AppearancePreferences::loadSettings(store);
        QCOMPARE(settings.theme, AppearancePreferences::Theme::Dark);
        QCOMPARE(settings.accent, AppearancePreferences::Accent::Ocean);
        QVERIFY(settings.glassEnabled);
        QVERIFY(store.allKeys().isEmpty());
    }

    void appearanceSettingsRoundTripIndependently() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("appearance.ini"));
        for (const auto theme : AppearancePreferences::themes()) {
            for (const auto accent : AppearancePreferences::accents()) {
                for (const bool glass : {false, true}) {
                    {
                        QSettings store(path, QSettings::IniFormat);
                        AppearancePreferences::saveSettings(store, {theme, accent, glass});
                        store.sync();
                        QCOMPARE(store.status(), QSettings::NoError);
                        QCOMPARE(store.value(QStringLiteral("appearance/glassEnabled")).toBool(),
                                 glass);
                    }
                    QSettings store(path, QSettings::IniFormat);
                    const auto settings = AppearancePreferences::loadSettings(store);
                    QCOMPARE(settings.theme, theme);
                    QCOMPARE(settings.accent, accent);
                    QCOMPARE(settings.glassEnabled, glass);
                }
            }
        }
    }

    void invalidAccentPreservesOtherSettingsAndLegacyThemeKey() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings store(directory.filePath(QStringLiteral("appearance.ini")), QSettings::IniFormat);
        store.setValue(QStringLiteral("appearance/theme"), QStringLiteral("light"));
        store.setValue(QStringLiteral("appearance/accent"), QStringLiteral("invalid"));
        store.setValue(QStringLiteral("appearance/glassEnabled"), false);
        auto settings = AppearancePreferences::loadSettings(store);
        QCOMPARE(settings.theme, AppearancePreferences::Theme::Light);
        QCOMPARE(settings.accent, AppearancePreferences::Accent::Ocean);
        QVERIFY(!settings.glassEnabled);
        store.setValue(QStringLiteral("appearance/theme"), QStringLiteral("invalid"));
        settings = AppearancePreferences::loadSettings(store);
        QCOMPARE(settings.theme, AppearancePreferences::Theme::Dark);
    }

    void savesStableAppearanceKeysWithoutReplacingOtherPreferences() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings store(directory.filePath(QStringLiteral("appearance.ini")), QSettings::IniFormat);
        store.setValue(QStringLiteral("appearance/language"), QStringLiteral("en"));
        AppearancePreferences::saveSettings(store, {AppearancePreferences::Theme::System,
                                                    AppearancePreferences::Accent::Violet, false});
        QCOMPARE(store.value(QStringLiteral("appearance/theme")).toString(),
                 QStringLiteral("system"));
        QCOMPARE(store.value(QStringLiteral("appearance/accent")).toString(),
                 QStringLiteral("violet"));
        QVERIFY(!store.value(QStringLiteral("appearance/glassEnabled")).toBool());
        QCOMPARE(store.value(QStringLiteral("appearance/language")).toString(),
                 QStringLiteral("en"));
    }

    void appliesSettingsAcrossThemesAndRestoresFullSystemPalette() {
        const auto previousPalette = qApp->palette();
        const auto previousStyle = qApp->styleSheet();
        for (const auto theme : AppearancePreferences::themes()) {
            AppearancePreferences::applySettings(
                *qApp, {theme, AppearancePreferences::Accent::Rose, true});
            const auto palette = qApp->palette();
            const bool dark = palette.color(QPalette::Window).lightnessF() < 0.5;
            if (theme == AppearancePreferences::Theme::System) {
                const auto standard = qApp->style()->standardPalette();
                const auto tokens = appearanceTokens(theme, AppearancePreferences::Accent::Rose);
                for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
                    for (int role = 0; role < QPalette::NColorRoles; ++role)
                        if (role != QPalette::Highlight && role != QPalette::HighlightedText &&
                            role != QPalette::Link)
                            QCOMPARE(palette.brush(group, QPalette::ColorRole(role)),
                                     standard.brush(group, QPalette::ColorRole(role)));
                for (const auto group : {QPalette::Active, QPalette::Inactive}) {
                    QCOMPARE(palette.color(group, QPalette::Highlight), tokens.selection);
                    QCOMPARE(palette.color(group, QPalette::HighlightedText), tokens.selectedText);
                    QCOMPARE(palette.color(group, QPalette::Link), tokens.accent);
                }
                // Disabled keeps the platform's own selection/link semantics.
                QCOMPARE(palette.color(QPalette::Disabled, QPalette::Highlight),
                         standard.color(QPalette::Disabled, QPalette::Highlight));
                QCOMPARE(palette.color(QPalette::Disabled, QPalette::HighlightedText),
                         standard.color(QPalette::Disabled, QPalette::HighlightedText));
                QCOMPARE(palette.color(QPalette::Disabled, QPalette::Link),
                         standard.color(QPalette::Disabled, QPalette::Link));
            } else {
                QCOMPARE(palette.color(QPalette::Link), theme == AppearancePreferences::Theme::Dark
                                                            ? QColor("#ec96b7")
                                                            : QColor("#b33c6d"));
            }
            QVERIFY(qApp->styleSheet().contains(QStringLiteral("rgba(")));
            QVERIFY(qApp->styleSheet().contains(dark ? QStringLiteral("#ec96b7")
                                                     : QStringLiteral("#b33c6d")));
            AppearancePreferences::applySettings(
                *qApp, {theme, AppearancePreferences::Accent::Emerald, false});
            QVERIFY(!qApp->styleSheet().contains(QStringLiteral("rgba(")));
            QVERIFY(qApp->styleSheet().contains(dark ? QStringLiteral("#73cfb0")
                                                     : QStringLiteral("#187653")));
        }
        qApp->setStyleSheet(previousStyle);
        qApp->setPalette(previousPalette);
    }

    void accentKeysRoundTripAndUnknownKeysFallBackToOcean() {
        using Accent = AppearancePreferences::Accent;
        QCOMPARE(AppearancePreferences::accents().size(), 5);
        const QStringList keys{QStringLiteral("ocean"), QStringLiteral("emerald"),
                               QStringLiteral("violet"), QStringLiteral("rose"),
                               QStringLiteral("amber")};
        for (qsizetype i = 0; i < keys.size(); ++i) {
            const auto accent = AppearancePreferences::accents().at(i);
            QCOMPARE(AppearancePreferences::accentKey(accent), keys.at(i));
            QCOMPARE(AppearancePreferences::accentFromKey(keys.at(i)), accent);
            QVERIFY(!AppearancePreferences::accentName(accent).isEmpty());
        }
        QCOMPARE(AppearancePreferences::accentFromKey({}), Accent::Ocean);
        QCOMPARE(AppearancePreferences::accentFromKey(QStringLiteral("unknown")), Accent::Ocean);
    }

    void accentsShareBaseSurfacesAndProvideDistinctInteractiveColors() {
        for (const auto theme :
             {AppearancePreferences::Theme::Dark, AppearancePreferences::Theme::Light}) {
            const auto base = appearanceTokens(theme);
            QList<QColor> accents;
            QList<QColor> selections;
            for (const auto accent : AppearancePreferences::accents()) {
                const auto tokens = appearanceTokens(theme, accent);
                QCOMPARE(tokens.canvas, base.canvas);
                QCOMPARE(tokens.surface, base.surface);
                QCOMPARE(tokens.raisedSurface, base.raisedSurface);
                QCOMPARE(tokens.primaryText, base.primaryText);
                QVERIFY(!accents.contains(tokens.accent));
                accents.append(tokens.accent);
                QVERIFY(!selections.contains(tokens.selection));
                selections.append(tokens.selection);
                QCOMPARE(tokens.focus, tokens.accent);
                QVERIFY(tokens.accentHover != tokens.accent);
                QVERIFY(tokens.accentPressed != tokens.accent);
            }
        }
    }

    void systemTokensUseStandardPaletteAndSelectedAccent() {
        const auto previous = qApp->palette();
        qApp->setPalette(paletteForTokens(appearanceTokens(AppearancePreferences::Theme::Dark)));
        const auto standard = qApp->style()->standardPalette();
        QList<QColor> accents;
        for (const auto accent : AppearancePreferences::accents()) {
            const auto tokens = appearanceTokens(AppearancePreferences::Theme::System, accent);
            QCOMPARE(tokens.canvas, standard.color(QPalette::Window));
            QCOMPARE(tokens.surface, standard.color(QPalette::Base));
            QCOMPARE(tokens.raisedSurface, standard.color(QPalette::AlternateBase));
            QCOMPARE(tokens.primaryText, standard.color(QPalette::WindowText));
            QVERIFY(!accents.contains(tokens.accent));
            accents.append(tokens.accent);
        }
        qApp->setPalette(previous);
    }

    void glassStylesUseTranslucentUiPanelsAndOpaqueFallback() {
        for (const auto theme :
             {AppearancePreferences::Theme::Dark, AppearancePreferences::Theme::Light,
              AppearancePreferences::Theme::System}) {
            const auto tokens = appearanceTokens(theme);
            const auto glass = appearanceStyleSheet(tokens, true);
            const auto opaque = appearanceStyleSheet(tokens, false);
            for (const auto &selector :
                 {QStringLiteral("QWidget#playlistPane"), QStringLiteral("QWidget#transportBar"),
                  QStringLiteral("QWidget#welcomePanel"), QStringLiteral("QWidget#playlistHeader"),
                  QStringLiteral("QPushButton[welcomeRecentAction]"),
                  QStringLiteral("QPushButton[welcomeContinueAction]"), QStringLiteral("QMenu{"),
                  QStringLiteral("QDialog{")}) {
                QVERIFY2(glass.contains(selector), qPrintable(selector));
                const auto rule = glass.mid(glass.indexOf(selector)).section('}', 0, 0);
                const QRegularExpression rgba(QStringLiteral("rgba\\(\\d+,\\d+,\\d+,(\\d+)\\)"));
                const auto match = rgba.match(rule);
                QVERIFY(match.hasMatch());
                QVERIFY(match.captured(1).toInt() > 0);
                QVERIFY(match.captured(1).toInt() < 255);
            }
            QVERIFY(glass.contains(QStringLiteral("qlineargradient(")));
            QVERIFY(!opaque.contains(QStringLiteral("rgba(")));
            QVERIFY(!opaque.contains(QStringLiteral("qlineargradient(")));
            for (const auto &style : {glass, opaque}) {
                QVERIFY(!style.contains(QStringLiteral("VideoWidget")));
                QVERIFY(!style.contains(QStringLiteral("QOpenGLWidget")));
                QVERIFY(!style.contains(QStringLiteral("PlaybackOverlay")));
                QVERIFY(!style.contains(QStringLiteral("ActionFeedbackOverlay")));
                QVERIFY(!style.contains(QStringLiteral("QWidget{")));
            }
        }
    }

    void darkAndLightTokensKeepSemanticContrastAndSharedMetrics() {
        const auto dark = appearanceTokens(AppearancePreferences::Theme::Dark);
        const auto light = appearanceTokens(AppearancePreferences::Theme::Light);
        QVERIFY(dark.canvas.value() < dark.primaryText.value());
        QVERIFY(light.canvas.value() > light.primaryText.value());
        QVERIFY(dark.accent != dark.canvas);
        QVERIFY(light.accent != light.canvas);
        QCOMPARE(dark.spaceUnit, light.spaceUnit);
        QCOMPARE(dark.transportHitTarget, 32);
    }

    void primaryActionForegroundContrastsWithAccentSurface() {
        const auto contrast = [](const QColor &first, const QColor &second) {
            const auto luminance = [](const QColor &color) {
                const auto linear = [](qreal channel) {
                    return channel <= 0.04045 ? channel / 12.92
                                              : std::pow((channel + 0.055) / 1.055, 2.4);
                };
                return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
                       0.0722 * linear(color.blueF());
            };
            const auto lighter = std::max(luminance(first), luminance(second));
            const auto darker = std::min(luminance(first), luminance(second));
            return (lighter + 0.05) / (darker + 0.05);
        };
        for (const auto theme :
             {AppearancePreferences::Theme::Dark, AppearancePreferences::Theme::Light}) {
            for (const auto accent : AppearancePreferences::accents()) {
                const auto tokens = appearanceTokens(theme, accent);
                QVERIFY2(contrast(tokens.accentText, tokens.accent) >= 4.5,
                         "Primary-action text must meet WCAG AA contrast");
                QVERIFY2(contrast(tokens.accentText, tokens.accentHover) >= 4.5,
                         "Hovered primary-action text must meet WCAG AA contrast");
                QVERIFY2(contrast(tokens.accentText, tokens.accentPressed) >= 4.5,
                         "Pressed primary-action text must meet WCAG AA contrast");
                QVERIFY2(contrast(tokens.selectedText, tokens.selection) >= 4.5,
                         "Selection text must meet WCAG AA contrast");
                QVERIFY2(contrast(tokens.mutedText, tokens.canvas) >= 4.5,
                         "Muted text must meet WCAG AA contrast on the canvas");
                QVERIFY2(contrast(tokens.mutedText, tokens.surface) >= 4.5,
                         "Muted text must meet WCAG AA contrast on surfaces");
                const auto style = appearanceStyleSheet(tokens);
                QVERIFY(style.contains(
                    QStringLiteral("QToolButton#transportPrimaryButton:checked{color:%1;")
                        .arg(tokens.accentText.name(QColor::HexRgb))));
            }
        }
    }

    void exposesThreeThemesAndAppliesPalettes() {
        QCOMPARE(AppearancePreferences::themes().size(), 3);
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::Dark);
        QVERIFY(qApp->palette().color(QPalette::Window).value() < 80);
        QVERIFY(!qApp->styleSheet().isEmpty());

        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::Light);
        QVERIFY(qApp->palette().color(QPalette::Window).value() > 220);

        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::System);
        QVERIFY(qApp->styleSheet().isEmpty());
    }

    void loadsBundledEnglishTranslations() {
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(":/i18n/NekoPlayer_en.qm")));
        qApp->installTranslator(&translator);
        QCOMPARE(AppearancePreferences::themeName(AppearancePreferences::Theme::Dark),
                 QStringLiteral("Dark"));
        QCOMPARE(AppearancePreferences::themeName(AppearancePreferences::Theme::Light),
                 QStringLiteral("Light"));
        qApp->removeTranslator(&translator);
    }

    void exposesContentFirstInteractionStyle() {
        const auto darkTokens = appearanceTokens(AppearancePreferences::Theme::Dark);
        const auto style = appearanceStyleSheet(darkTokens);
        QVERIFY(style.contains(QStringLiteral("QWidget#playlistPane")));
        QVERIFY(style.contains(QStringLiteral("QWidget#transportBar")));
        QVERIFY(style.contains(QStringLiteral("QFrame#transportDivider")));
        QVERIFY(style.contains(QStringLiteral("QLabel#timeLabel")));
        QVERIFY(style.contains(QStringLiteral("QSlider#volumeSlider")));
        QVERIFY(style.contains(QStringLiteral("QWidget#welcomePanel")));
        QVERIFY(style.contains(QStringLiteral("QLabel#playlistCount")));
        QVERIFY(style.contains(QStringLiteral("QPushButton#welcomeOpenButton")));
        QVERIFY(style.contains(QStringLiteral("QSlider#timelineSlider")));
        QVERIFY(style.contains(QStringLiteral("QToolButton:pressed")));
        QVERIFY(style.contains(QStringLiteral("QToolButton:checked")));
        QVERIFY(style.contains(QStringLiteral("QToolButton#transportPrimaryButton:hover")));
        QVERIFY(style.contains(QStringLiteral("QToolButton#transportPrimaryButton:checked")));
        QVERIFY(style.contains(QStringLiteral("QListView#playlistView::item:hover")));
        QVERIFY(style.contains(QStringLiteral("QListView#playlistView::item:selected")));
        QVERIFY(style.contains(QStringLiteral("QPushButton:focus")));
        QVERIFY(style.contains(QStringLiteral("QPushButton:pressed")));
        QVERIFY(style.contains(QStringLiteral("QSlider:focus")));
        QVERIFY(style.contains(QStringLiteral("QSlider::handle:horizontal:pressed")));
        QVERIFY(style.contains(QStringLiteral("QSlider::handle:horizontal:disabled")));
        QVERIFY(style.contains(QStringLiteral("QMenu::item:disabled")));
        QVERIFY(style.contains(QStringLiteral("QFocusFrame")));
        QVERIFY(style.contains(QStringLiteral("QToolButton:disabled")));
        QVERIFY(style.contains(QStringLiteral("QPushButton:disabled")));
        QVERIFY(style.contains(QStringLiteral("QToolButton:hover")));
        QVERIFY(style.contains(QStringLiteral("QToolButton#transportPrimaryButton:pressed")));
        QVERIFY(style.contains(QStringLiteral("QListView#playlistView:focus")));

        const auto lightTokens = appearanceTokens(AppearancePreferences::Theme::Light);
        const auto lightStyle = appearanceStyleSheet(lightTokens);
        QVERIFY(lightStyle.contains(QStringLiteral("QToolButton:pressed")));
        QVERIFY(lightStyle.contains(QStringLiteral("QToolButton#transportPrimaryButton:hover")));
        QVERIFY(lightStyle.contains(QStringLiteral("QToolButton#transportPrimaryButton:checked")));
        QVERIFY(lightStyle.contains(QStringLiteral("QListView#playlistView::item:selected")));
        QVERIFY(lightStyle.contains(QStringLiteral("QSlider#timelineSlider")));
        QVERIFY(lightStyle.contains(QStringLiteral("QWidget#welcomePanel")));
        QVERIFY(lightStyle.contains(QStringLiteral("QFrame#transportDivider")));
        QVERIFY(lightStyle.contains(QStringLiteral("QLabel#timeLabel")));
        QVERIFY(lightStyle.contains(QStringLiteral("QSlider#volumeSlider")));
        QVERIFY(lightStyle.contains(QStringLiteral("QPushButton#welcomeOpenButton")));
        QVERIFY(lightStyle.contains(QStringLiteral("QPushButton:pressed")));
        QVERIFY(lightStyle.contains(QStringLiteral("QSlider::handle:horizontal:pressed")));
        QVERIFY(lightStyle.contains(QStringLiteral("QSlider::handle:horizontal:disabled")));
        QVERIFY(lightStyle.contains(QStringLiteral("QToolButton:disabled")));
        QVERIFY(lightStyle.contains(QStringLiteral("QPushButton:disabled")));
        QVERIFY(lightStyle.contains(QStringLiteral("QToolButton:hover")));
        QVERIFY(lightStyle.contains(QStringLiteral("QToolButton:checked")));
        QVERIFY(lightStyle.contains(QStringLiteral("QToolButton#transportPrimaryButton:pressed")));
        QVERIFY(lightStyle.contains(QStringLiteral("QListView#playlistView:focus")));
    }
};

QTEST_MAIN(AppearancePreferencesTest)
#include "appearance_preferences_test.moc"

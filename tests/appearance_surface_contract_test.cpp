#include "ui/appearancepreferences.h"
#include "ui/appearancestylesheet.h"
#include "ui/appearancetokens.h"

#include <QApplication>
#include <QtTest>

#include <algorithm>
#include <cmath>

class AppearanceSurfaceContractTest final : public QObject {
    Q_OBJECT
  private slots:
    void tokensExposeStableSurfaceHierarchy() {
        const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark,
                                             AppearancePreferences::Accent::Violet);
        QVERIFY(tokens.canvas.isValid());
        QVERIFY(tokens.surface.isValid());
        QVERIFY(tokens.raisedSurface.isValid());
        QVERIFY(tokens.accentSurface.isValid());
        QVERIFY(tokens.border.isValid());
        QCOMPARE(tokens.focus, tokens.accent);
        QVERIFY(tokens.controlHeight >= tokens.rowHeight);
        QVERIFY(tokens.menuItemHeight >= tokens.rowHeight);
        QCOMPARE(tokens.controlHeight % tokens.spaceUnit, 0);
        QCOMPARE(tokens.rowHeight % tokens.spaceUnit, 0);
        QVERIFY(tokens.motionDurationMs >= 140);
        QVERIFY(tokens.motionDurationMs <= 180);
    }

    void elevatedPopupsUseRaisedSurfaceMaterial() {
        for (const auto theme :
             {AppearancePreferences::Theme::Dark, AppearancePreferences::Theme::Light,
              AppearancePreferences::Theme::System}) {
            const auto tokens = appearanceTokens(theme);
            const auto glass =
                appearanceStyleSheet(tokens, true, glassOpacityDefault, glassBlurDefault);
            const auto opaque = appearanceStyleSheet(tokens, false);
            const auto raisedCenter = QStringLiteral("rgba(%1,%2,%3,")
                                          .arg(tokens.raisedSurface.red())
                                          .arg(tokens.raisedSurface.green())
                                          .arg(tokens.raisedSurface.blue());
            const auto surfaceCenter = QStringLiteral("rgba(%1,%2,%3,")
                                           .arg(tokens.surface.red())
                                           .arg(tokens.surface.green())
                                           .arg(tokens.surface.blue());
            const auto panel =
                glass.mid(glass.indexOf(QStringLiteral("QWidget#transportBar"))).section('}', 0, 0);
            QVERIFY(panel.contains(surfaceCenter));

            for (const auto &selector :
                 {QStringLiteral("QMenu{"), QStringLiteral("QDialog{"),
                  QStringLiteral("QComboBox QAbstractItemView{"), QStringLiteral("QToolTip{")}) {
                const auto popup = glass.mid(glass.indexOf(selector)).section('}', 0, 0);
                QVERIFY2(popup.contains(raisedCenter), qPrintable(selector));
            }
            const auto opaqueMenu =
                opaque.mid(opaque.indexOf(QStringLiteral("QMenu{"))).section('}', 0, 0);
            QVERIFY(opaqueMenu.contains(tokens.raisedSurface.name(QColor::HexRgb)));
        }
    }

    void textTokensMeetContrastOnEveryDarkAndLightSurface() {
        const auto luminance = [](const QColor &color) {
            const auto linear = [](qreal channel) {
                return channel <= 0.04045 ? channel / 12.92
                                          : std::pow((channel + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
                   0.0722 * linear(color.blueF());
        };
        for (const auto theme :
             {AppearancePreferences::Theme::Dark, AppearancePreferences::Theme::Light}) {
            const auto tokens = appearanceTokens(theme);
            const auto check = [&](const QColor &textColor, const QColor &surface,
                                   const char *description) {
                const auto high = std::max(luminance(textColor), luminance(surface));
                const auto low = std::min(luminance(textColor), luminance(surface));
                QVERIFY2((high + 0.05) / (low + 0.05) >= 4.5, description);
            };
            for (const auto &surface : {tokens.canvas, tokens.surface, tokens.raisedSurface,
                                        tokens.hoverSurface, tokens.pressedSurface}) {
                check(tokens.primaryText, surface, "Primary text must meet WCAG AA contrast");
                check(tokens.secondaryText, surface, "Secondary text must meet WCAG AA contrast");
                check(tokens.mutedText, surface, "Muted text must meet WCAG AA contrast");
            }
        }
    }
};

QTEST_MAIN(AppearanceSurfaceContractTest)
#include "appearance_surface_contract_test.moc"

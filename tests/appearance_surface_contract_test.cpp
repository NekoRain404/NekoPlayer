#include "ui/appearancepreferences.h"
#include "ui/appearancestylesheet.h"
#include "ui/appearancetokens.h"

#include <QApplication>
#include <QComboBox>
#include <QImage>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
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

    void keyboardFocusRingIsExplicitAndContrastsAcrossAccents() {
        const auto style =
            appearanceStyleSheet(appearanceTokens(AppearancePreferences::Theme::Dark));
        QVERIFY(style.contains(QStringLiteral("QCheckBox:focus::indicator")));

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
            for (const auto accent : AppearancePreferences::accents()) {
                const auto tokens = appearanceTokens(theme, accent);
                for (const auto &surface : {tokens.canvas, tokens.surface, tokens.raisedSurface}) {
                    const auto high = std::max(luminance(tokens.focus), luminance(surface));
                    const auto low = std::min(luminance(tokens.focus), luminance(surface));
                    QVERIFY2((high + 0.05) / (low + 0.05) >= 3.0,
                             "Focus boundary must meet UI contrast on core surfaces");
                }
            }
        }
    }

    void focusedSliderHandleHasVisibleFeedback() {
        const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark,
                                             AppearancePreferences::Accent::Ocean);
        QSlider slider(Qt::Horizontal);
        slider.setRange(0, 100);
        slider.setValue(50);
        slider.resize(240, 32);
        slider.setStyleSheet(appearanceStyleSheet(tokens, false));
        slider.show();
        QCoreApplication::processEvents();

        QImage unfocused(slider.size(), QImage::Format_ARGB32_Premultiplied);
        unfocused.fill(Qt::transparent);
        slider.render(&unfocused);

        slider.setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        QVERIFY(slider.hasFocus());
        QImage focused(slider.size(), QImage::Format_ARGB32_Premultiplied);
        focused.fill(Qt::transparent);
        slider.render(&focused);

        QVERIFY(focused != unfocused);
    }

    void disabledFormControlsHaveVisibleFeedback() {
        const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark);
        const auto style = appearanceStyleSheet(tokens, false);
        const auto render = [](QWidget &widget) {
            QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            widget.render(&painter);
            return image;
        };

        QLineEdit edit(QStringLiteral("Example"));
        QComboBox combo;
        combo.addItem(QStringLiteral("Example"));
        QSpinBox spin;
        QSlider slider(Qt::Horizontal);
        slider.setRange(0, 100);
        slider.setValue(50);
        QList<QWidget *> controls{&edit, &combo, &spin, &slider};
        for (auto *control : controls) {
            control->setStyleSheet(style);
            control->resize(180, 36);
            control->show();
            QCoreApplication::processEvents();
            const auto enabled = render(*control);
            control->setEnabled(false);
            const auto disabled = render(*control);
            QVERIFY2(disabled != enabled, control->metaObject()->className());
        }
    }

    void focusedFormControlsHaveVisibleFeedback() {
        const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark);
        const auto style = appearanceStyleSheet(tokens, false);
        QWidget host;
        auto *edit = new QLineEdit(QStringLiteral("Example"), &host);
        auto *combo = new QComboBox(&host);
        combo->addItem(QStringLiteral("Example"));
        auto *spin = new QSpinBox(&host);
        auto *button = new QPushButton(QStringLiteral("Example"), &host);
        auto *toolButton = new QToolButton(&host);
        QList<QWidget *> controls{edit, combo, spin, button, toolButton};
        int top = 0;
        for (auto *control : controls) {
            control->setStyleSheet(style);
            control->setGeometry(0, top, 180, 36);
            top += 40;
        }
        host.resize(200, top);
        host.setFocusPolicy(Qt::StrongFocus);
        host.show();
        host.activateWindow();
        host.setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();

        for (auto *control : controls) {
            QImage unfocused(control->size(), QImage::Format_ARGB32_Premultiplied);
            unfocused.fill(Qt::transparent);
            control->render(&unfocused);
            control->setFocus(Qt::OtherFocusReason);
            QCoreApplication::processEvents();
            QVERIFY2(control->hasFocus(), control->metaObject()->className());
            QImage focused(control->size(), QImage::Format_ARGB32_Premultiplied);
            focused.fill(Qt::transparent);
            control->render(&focused);
            QVERIFY2(focused != unfocused, control->metaObject()->className());
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

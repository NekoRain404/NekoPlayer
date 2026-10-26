#include "preferencesformlayout.h"
#include "appearancetokens.h"

#include <QFormLayout>
#include <QLabel>

void configurePreferenceForm(QFormLayout *form, const AppearanceTokens &tokens) {
    form->setContentsMargins(tokens.spaceUnit * 3, tokens.spaceUnit * 3, tokens.spaceUnit * 3,
                             tokens.spaceUnit * 3);
    form->setHorizontalSpacing(tokens.spaceUnit * 4);
    form->setVerticalSpacing(tokens.spaceUnit * 3);
}

void alignPreferenceFormLabels(QWidget *root, const AppearanceTokens &tokens) {
    const int labelWidth = tokens.spaceUnit * 32;
    for (auto *form : root->findChildren<QFormLayout *>()) {
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        for (int row = 0; row < form->rowCount(); ++row) {
            auto *item = form->itemAt(row, QFormLayout::LabelRole);
            auto *label = item ? qobject_cast<QLabel *>(item->widget()) : nullptr;
            if (!label)
                continue;
            label->setFixedWidth(labelWidth);
            label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
    }
}

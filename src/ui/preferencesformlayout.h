#pragma once

class AppearanceTokens;
class QFormLayout;
class QWidget;

void configurePreferenceForm(QFormLayout *form, const AppearanceTokens &tokens);
void alignPreferenceFormLabels(QWidget *root, const AppearanceTokens &tokens);

#pragma once

#include "Shared.h"

#include <QtCore/QMap>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>

class WIDGETS_EXPORT MirrorDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit MirrorDialog(QWidget* parent = 0);

    private:
        QComboBox* sourceDevice;
        QSpinBox* sourceChannel;
        QComboBox* targetDevice;
        QSpinBox* targetChannel;
        QCheckBox* enabled;
        QMap<QString, QCheckBox*> verbs;
        QLabel* errorLabel;

        Q_SLOT void accept();
};

#pragma once

#include "Shared.h"

#include <QtCore/QTimer>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

// Indicadores verde/vermelho dos 2 servidores + chave do Mirror + engrenagem.
class WIDGETS_EXPORT MirrorStatusWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit MirrorStatusWidget(QWidget* parent = 0);

    private:
        QLabel* sourceLed;
        QLabel* targetLed;
        QCheckBox* toggle;
        QToolButton* gear;
        QTimer timer;

        Q_SLOT void refresh();
        Q_SLOT void toggled(bool checked);
        Q_SLOT void openSettings();
};

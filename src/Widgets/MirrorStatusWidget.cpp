#include "MirrorStatusWidget.h"
#include "MirrorDialog.h"

#include "MirrorManager.h"

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMessageBox>

namespace
{
    QString ledStyle(bool ok)
    {
        return QString("QLabel { color: %1; font-weight: bold; }").arg(ok ? "#2ecc71" : "#e74c3c");
    }
}

MirrorStatusWidget::MirrorStatusWidget(QWidget* parent)
    : QWidget(parent)
{
    this->sourceLed = new QLabel(this);
    this->targetLed = new QLabel(this);

    this->toggle = new QCheckBox(tr("Mirror"), this);

    this->gear = new QToolButton(this);
    this->gear->setText(QString::fromUtf8("\xE2\x9A\x99")); // engrenagem
    this->gear->setAutoRaise(true);
    this->gear->setToolTip(tr("Configurar Mirror"));

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 0, 6, 0);
    layout->addWidget(this->sourceLed);
    layout->addWidget(this->targetLed);
    layout->addWidget(this->toggle);
    layout->addWidget(this->gear);

    QObject::connect(this->toggle, SIGNAL(toggled(bool)), this, SLOT(toggled(bool)));
    QObject::connect(this->gear, SIGNAL(clicked()), this, SLOT(openSettings()));
    QObject::connect(&MirrorManager::getInstance(), SIGNAL(settingsChanged()), this, SLOT(refresh()));
    QObject::connect(&this->timer, SIGNAL(timeout()), this, SLOT(refresh()));

    this->timer.start(500);
    refresh();
}

void MirrorStatusWidget::refresh()
{
    MirrorManager& mm = MirrorManager::getInstance();
    const MirrorManager::Settings s = mm.getSettings();

    const bool src = mm.isSourceConnected();
    const bool dst = mm.isTargetConnected();

    const QString srcName = s.sourceDevice.isEmpty() ? tr("S1") : s.sourceDevice;
    const QString dstName = s.targetDevice.isEmpty() ? tr("S2") : s.targetDevice;

    this->sourceLed->setText(QString::fromUtf8("\xE2\x97\x8F ") + srcName);
    this->targetLed->setText(QString::fromUtf8("\xE2\x97\x8F ") + dstName);
    this->sourceLed->setStyleSheet(ledStyle(src));
    this->targetLed->setStyleSheet(ledStyle(dst));

    this->sourceLed->setToolTip(src ? tr("Conectado") : tr("Desconectado"));
    this->targetLed->setToolTip(dst ? tr("Conectado") : tr("Desconectado"));

    {
        const QSignalBlocker blocker(this->toggle);
        this->toggle->setChecked(s.enabled);
    }

    this->toggle->setToolTip(tr("Espelhados: %1  |  Descartados (destino offline): %2")
                                 .arg(mm.getMirroredCount()).arg(mm.getDroppedCount()));
}

void MirrorStatusWidget::toggled(bool checked)
{
    MirrorManager& mm = MirrorManager::getInstance();
    mm.setEnabled(checked);

    if (checked && !mm.getSettings().enabled)
    {
        // Configuracao invalida: abre a janela para o usuario corrigir.
        refresh();
        openSettings();
    }
}

void MirrorStatusWidget::openSettings()
{
    MirrorDialog dialog(this);
    dialog.exec();
    refresh();
}

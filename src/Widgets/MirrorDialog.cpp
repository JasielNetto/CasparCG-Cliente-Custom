#include "MirrorDialog.h"

#include "DeviceManager.h"
#include "MirrorManager.h"
#include "Models/DeviceModel.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QVBoxLayout>

MirrorDialog::MirrorDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Mirror"));
    setModal(true);

    const MirrorManager::Settings s = MirrorManager::getInstance().getSettings();

    this->sourceDevice = new QComboBox(this);
    this->targetDevice = new QComboBox(this);
    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        this->sourceDevice->addItem(model.getName());
        this->targetDevice->addItem(model.getName());
    }
    this->sourceDevice->setCurrentIndex(qMax(0, this->sourceDevice->findText(s.sourceDevice)));
    this->targetDevice->setCurrentIndex(qMax(0, this->targetDevice->findText(s.targetDevice)));

    this->sourceChannel = new QSpinBox(this);
    this->sourceChannel->setRange(1, 99);
    this->sourceChannel->setValue(s.sourceChannel);
    this->targetChannel = new QSpinBox(this);
    this->targetChannel->setRange(1, 99);
    this->targetChannel->setValue(s.targetChannel);

    QGroupBox* sourceBox = new QGroupBox(tr("Origem (Servidor 1)"), this);
    QFormLayout* sourceLayout = new QFormLayout(sourceBox);
    sourceLayout->addRow(tr("Servidor"), this->sourceDevice);
    sourceLayout->addRow(tr("Canal"), this->sourceChannel);

    QGroupBox* targetBox = new QGroupBox(tr("Destino (Servidor 2)"), this);
    QFormLayout* targetLayout = new QFormLayout(targetBox);
    targetLayout->addRow(tr("Servidor"), this->targetDevice);
    targetLayout->addRow(tr("Canal"), this->targetChannel);

    QGroupBox* verbBox = new QGroupBox(tr("Comandos espelhados"), this);
    QGridLayout* verbLayout = new QGridLayout(verbBox);
    int i = 0;
    foreach (const QString& name, MirrorManager::verbNames())
    {
        QCheckBox* box = new QCheckBox(name, verbBox);
        box->setChecked((s.verbs & MirrorManager::verbsFromString(name)) != 0);
        this->verbs.insert(name, box);
        verbLayout->addWidget(box, i / 3, i % 3);
        ++i;
    }

    this->enabled = new QCheckBox(tr("Mirror ativo"), this);
    this->enabled->setChecked(s.enabled);

    this->errorLabel = new QLabel(this);
    this->errorLabel->setStyleSheet("color: #d33;");
    this->errorLabel->setWordWrap(true);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
    QObject::connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(sourceBox);
    layout->addWidget(targetBox);
    layout->addWidget(verbBox);
    layout->addWidget(this->enabled);
    layout->addWidget(this->errorLabel);
    layout->addWidget(buttons);
}

void MirrorDialog::accept()
{
    MirrorManager::Settings s;
    s.enabled = this->enabled->isChecked();
    s.sourceDevice = this->sourceDevice->currentText();
    s.sourceChannel = this->sourceChannel->value();
    s.targetDevice = this->targetDevice->currentText();
    s.targetChannel = this->targetChannel->value();

    s.verbs = 0;
    for (auto it = this->verbs.constBegin(); it != this->verbs.constEnd(); ++it)
    {
        if (it.value()->isChecked())
            s.verbs |= MirrorManager::verbsFromString(it.key());
    }

    const QString error = MirrorManager::getInstance().validate(s);
    if (!error.isEmpty())
    {
        this->errorLabel->setText(error);
        return;
    }

    MirrorManager::getInstance().setSettings(s);

    QDialog::accept();
}

#pragma once

// Configuração da estação: servidores, vínculo de VT / Selo 1 / Selo 2 e porta OSC.
// A validação do núcleo roda a cada alteração; "Salvar" só fica disponível sem erros.

#include "StationConfig.h"

#include <QtCore/QMap>
#include <QtWidgets/QDialog>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QSpinBox;
class QTableWidget;

class StationDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit StationDialog(const engine::StationConfig& config, QWidget* parent = nullptr);

        engine::StationConfig config() const;

    private:
        struct ResourceRow
        {
            QComboBox* server = nullptr;
            QSpinBox* channel = nullptr;
            QSpinBox* layer = nullptr;
        };

        QTableWidget* servers = nullptr;
        QMap<engine::LogicalResource, ResourceRow> resourceRows;
        QSpinBox* oscPort = nullptr;
        QLabel* issues = nullptr;
        QDialogButtonBox* buttons = nullptr;

        void addServerRow(const engine::ServerConfig& server);
        void refreshServerChoices();
        void validate();
};

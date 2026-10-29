#pragma once

// Biblioteca de mídia (RF-01): pastas e arquivos do servidor escolhido, com busca.
// Arrastar para o Rundown cria itens; duplo clique adiciona no ponto selecionado do Rundown.

#include <QtCore/QStringList>
#include <QtWidgets/QWidget>

class ConnectionManager;
class QComboBox;
class QLineEdit;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTreeView;

class MediaLibraryWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit MediaLibraryWidget(ConnectionManager* connections, QWidget* parent = nullptr);

        QStringList selectedMedia() const;

        Q_SIGNAL void mediaActivated(const QStringList& media);

    private:
        ConnectionManager* connections = nullptr;
        QComboBox* server = nullptr;
        QLineEdit* search = nullptr;
        QTreeView* view = nullptr;
        QStandardItemModel* items = nullptr;
        QSortFilterProxyModel* filter = nullptr;

        void reloadServers();
        void reloadMedia();
};

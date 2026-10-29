#pragma once

#include "StationConfig.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtWidgets/QMainWindow>

class ConnectionManager;
class ExecutionController;
class MediaLibraryWidget;
class QCloseEvent;
class QLabel;
class QSplitter;
class ResourcePanel;
class RundownModel;
class RundownWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow() override;

        bool openRundown(const QString& path);

    protected:
        void closeEvent(QCloseEvent* event) override;

    private:
        ConnectionManager* connections = nullptr;
        RundownModel* model = nullptr;
        ExecutionController* execution = nullptr;
        MediaLibraryWidget* library = nullptr;
        RundownWidget* rundown = nullptr;
        QList<ResourcePanel*> panels;
        QSplitter* splitter = nullptr;
        QLabel* serverStatus = nullptr;

        engine::StationConfig station;
        QString rundownPath;

        void buildMenus();
        void loadStation();
        void configureStation();
        void refreshServerStatus();
        void refreshTitle();

        bool confirmDiscard();
        void newRundown();
        void openRundownDialog();
        bool saveRundown();
        bool saveRundownAs();

        void restoreLayout();
        void saveLayout() const;
};

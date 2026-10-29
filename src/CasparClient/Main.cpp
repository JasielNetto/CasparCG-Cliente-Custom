// CasparClient — cliente de playout para CasparCG Server 2.4 (ver PROJECT_CONTEXT.md, D-08).

#include "MainWindow.h"

#include <QtCore/QStringList>
#include <QtGui/QColor>
#include <QtGui/QPalette>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

namespace
{
    void applyDarkTheme(QApplication& app)
    {
        app.setStyle(QStyleFactory::create("Fusion"));

        QPalette palette;
        const QColor window(37, 37, 38);
        const QColor base(28, 28, 30);
        const QColor text(220, 220, 220);
        const QColor highlight(42, 110, 187);

        palette.setColor(QPalette::Window, window);
        palette.setColor(QPalette::WindowText, text);
        palette.setColor(QPalette::Base, base);
        palette.setColor(QPalette::AlternateBase, window);
        palette.setColor(QPalette::ToolTipBase, window);
        palette.setColor(QPalette::ToolTipText, text);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::Button, QColor(48, 48, 50));
        palette.setColor(QPalette::ButtonText, text);
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Highlight, highlight);
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::PlaceholderText, QColor(130, 130, 130));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(110, 110, 110));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(110, 110, 110));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(110, 110, 110));
        app.setPalette(palette);
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("CasparClient");
    QApplication::setApplicationName("CasparClient");

    applyDarkTheme(app);

    MainWindow window;
    window.show();

    // casparclient <arquivo.ccrundown> abre o rundown direto.
    const QStringList arguments = QApplication::arguments();
    if (arguments.count() > 1)
        window.openRundown(arguments.at(1));

    return app.exec();
}

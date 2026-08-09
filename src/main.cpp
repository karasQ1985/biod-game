#include <QApplication>
#include <QSurfaceFormat>
#include <QCommandLineParser>
#include "ui/MainWindow.h"
#include "ui/GLWidget.h"

int main(int argc, char* argv[])
{
    // Request OpenGL 3.3 Core profile (must be before QApplication)
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    app.setApplicationName("Biod");

    // Command-line options
    QCommandLineParser parser;
    parser.setApplicationDescription("Flock behavior simulator - performance test");
    parser.addHelpOption();
    QCommandLineOption boidCountOption(
        QStringList() << "n" << "boids",
        "Initial boid count (default: 500). Use -n 5000 for performance test.",
        "count", "500");
    parser.addOption(boidCountOption);
    parser.process(app);

    int initialCount = parser.value(boidCountOption).toInt();
    if (initialCount < 0) initialCount = 500;
    if (initialCount > 10000) initialCount = 10000;

    MainWindow window;

    // Set initial boid count before GLWidget initializes
    auto* glWidget = window.findChild<GLWidget*>();
    if (glWidget) {
        glWidget->setInitialBoidCount(initialCount);
    }

    window.show();
    return app.exec();
}

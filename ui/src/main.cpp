// sirenproof-ui: the main window.
//
//   sirenproof-ui [--pipe <name>] [--screen now|rules|lock] [--palette <command>] [--run <command>]
//     --palette opens the command palette with that text; --run does the command as if typed there
//     (a lock still only fills in the lock screen)
//   sirenproof-ui --screenshot <file.png> [--screen ...]   render one screen offscreen and exit (development)

#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include <string_view>

#include "service_client.hpp"
#include "sirenproof/platform/pipe.hpp"
#include "sirenproof/platform/win.hpp"

namespace {

QString option(const QStringList& args, const QString& name) {
    const qsizetype i = args.indexOf(name);
    return i >= 0 && i + 1 < args.size() ? args.at(i + 1) : QString{};
}

}  // namespace

int main(int argc, char* argv[]) {
    // A screenshot run needs no screen and no GPU.
    bool screenshot = false;
    for (int i = 1; i < argc; ++i) screenshot = screenshot || std::string_view(argv[i]) == "--screenshot";
    if (screenshot) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // The offscreen platform has no font list of its own; give it Windows' fonts.
        wchar_t windows[MAX_PATH] = {};
        const UINT length = GetWindowsDirectoryW(windows, MAX_PATH);
        qputenv("QT_QPA_FONTDIR", (QString::fromWCharArray(windows, static_cast<qsizetype>(length)) + "\\Fonts").toUtf8());
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Sirenproof");
    QGuiApplication::setOrganizationName("Sirenproof");
    QQuickStyle::setStyle("Basic");
    // Every piece of text inherits this; Windows 11's variable Segoe where it exists.
    for (const QString& family : {QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI")}) {
        if (QFontDatabase::hasFamily(family)) {
            QFont font(family);
            font.setPixelSize(15);
            QGuiApplication::setFont(font);
            break;
        }
    }

    const QStringList args = QGuiApplication::arguments();
    const QString pipe = option(args, "--pipe");
    const QString screen = option(args, "--screen");

    sirenproof::ui::ServiceClient service(pipe.isEmpty() ? std::wstring(sirenproof::platform::kServicePipeName)
                                                         : pipe.toStdWString());
    qmlRegisterSingletonInstance("Sirenproof.Backend", 1, 0, "Service", &service);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"screen", screen.isEmpty() ? QStringLiteral("now") : screen},
                                 {"paletteText", option(args, "--palette")},
                                 {"runText", option(args, "--run")}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Sirenproof", "Main");

    if (screenshot) {
        const QString file = option(args, "--screenshot");
        QTimer::singleShot(1500, &app, [&engine, file] {
            const auto roots = engine.rootObjects();
            auto* window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
            const bool saved = window && window->grabWindow().save(file);
            QCoreApplication::exit(saved ? 0 : 2);
        });
    }
    return QGuiApplication::exec();
}

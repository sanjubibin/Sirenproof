#include "frame_meter.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSGRendererInterface>
#include <QTimer>

#include <algorithm>

#include "sirenproof/platform/win.hpp"

namespace sirenproof::ui {

namespace {

std::uint64_t ticks(const FILETIME& t) { return (std::uint64_t{t.dwHighDateTime} << 32) | t.dwLowDateTime; }

std::uint64_t now_ticks() {
    FILETIME now{};
    GetSystemTimePreciseAsFileTime(&now);
    return ticks(now);
}

double ms(std::uint64_t from, std::uint64_t to) { return static_cast<double>(to - from) / 10'000.0; }

QString graphics_name(QSGRendererInterface::GraphicsApi api) {
    switch (api) {
        case QSGRendererInterface::Software: return "software";
        case QSGRendererInterface::OpenGL: return "OpenGL";
        case QSGRendererInterface::Direct3D11: return "Direct3D 11";
        case QSGRendererInterface::Direct3D12: return "Direct3D 12";
        case QSGRendererInterface::Vulkan: return "Vulkan";
        default: return "other";
    }
}

}  // namespace

FrameMeter::FrameMeter(QQuickWindow* window, QString file, std::chrono::milliseconds span)
    : QObject(window), window_(window), file_(std::move(file)), span_(span), started_(0) {
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) started_ = ticks(creation);
    connect(window, &QQuickWindow::frameSwapped, this, &FrameMeter::swapped, Qt::DirectConnection);
    // Never hang: a window that doesn't draw at all is a result too.
    QTimer::singleShot(std::chrono::seconds{10}, this, [this] {
        const std::scoped_lock lock(mutex_);
        if (!frames_.empty()) return;
        QFile out(file_);
        if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) out.write(R"({"error":"no frame within 10 s"})");
        QCoreApplication::exit(3);
    });
}

void FrameMeter::swapped() {
    const std::uint64_t at = now_ticks();
    const std::scoped_lock lock(mutex_);
    frames_.push_back(at);
    if (frames_.size() == 1) {
        QMetaObject::invokeMethod(this, [this] { QTimer::singleShot(span_, this, &FrameMeter::finish); }, Qt::QueuedConnection);
    }
}

void FrameMeter::finish() {
    std::vector<std::uint64_t> frames;
    {
        const std::scoped_lock lock(mutex_);
        frames = frames_;
    }
    double worst = 0;
    for (std::size_t i = 1; i < frames.size(); ++i) worst = std::max(worst, ms(frames[i - 1], frames[i]));
    const double seconds = std::chrono::duration<double>(span_).count();
    const auto after_first = static_cast<double>(frames.empty() ? 0 : frames.size() - 1);
    QJsonObject result{{"first_frame_ms", frames.empty() || !started_ ? -1.0 : ms(started_, frames.front())},
                       {"frames", after_first},
                       {"seconds", seconds},
                       {"fps", after_first / seconds},
                       {"worst_gap_ms", worst},
                       {"graphics", graphics_name(window_->rendererInterface()->graphicsApi())}};
    QFile out(file_);
    const bool written = out.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                         out.write(QJsonDocument(result).toJson(QJsonDocument::Compact)) > 0;
    QCoreApplication::exit(written ? 0 : 2);
}

}  // namespace sirenproof::ui

#pragma once

#include <QObject>
#include <QQuickWindow>
#include <QString>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

namespace sirenproof::ui {

// A development check of the window's budgets (plan §3.7): how long from the process starting to its
// first frame, then how often it draws over `span`. Writes the numbers to `file` as JSON and quits.
class FrameMeter : public QObject {
public:
    FrameMeter(QQuickWindow* window, QString file, std::chrono::milliseconds span);

private:
    void swapped();  // on the render thread
    void finish();

    QQuickWindow* window_;
    QString file_;
    std::chrono::milliseconds span_;
    std::uint64_t started_;  // process creation, in 100 ns units
    std::mutex mutex_;
    std::vector<std::uint64_t> frames_;
};

}  // namespace sirenproof::ui

#include "pause_overlay.hpp"

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace sirenproof::tray {

using Microsoft::WRL::ComPtr;

namespace {

constexpr UINT_PTR kTickTimer = 1;
constexpr UINT kTickMs = 100;  // only while the countdown runs
constexpr BYTE kOpacity = 238;  // the program shows faintly through

// The design's colours (ui/qml/Theme.qml).
constexpr std::uint32_t kGround = 0x050b18;
constexpr std::uint32_t kAccent = 0x5ce1e6;
constexpr std::uint32_t kText = 0xe6f1ff;
constexpr std::uint32_t kMuted = 0x9fb6d6;
constexpr std::uint32_t kInk = 0x04101f;

D2D1_COLOR_F colour(std::uint32_t rgb, float alpha = 1.0f) { return D2D1::ColorF(rgb, alpha); }

std::wstring family(IDWriteFactory* dwrite, const wchar_t* preferred) {
    ComPtr<IDWriteFontCollection> fonts;
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (SUCCEEDED(dwrite->GetSystemFontCollection(&fonts)) && SUCCEEDED(fonts->FindFamilyName(preferred, &index, &exists)) &&
        exists) {
        return preferred;
    }
    return L"Segoe UI";
}

struct Text {
    ComPtr<IDWriteTextFormat> eyebrow, title, line, goal, big, small, button, footer;
};

ComPtr<IDWriteTextFormat> format(IDWriteFactory* dwrite, const std::wstring& face, float size, DWRITE_FONT_WEIGHT weight) {
    ComPtr<IDWriteTextFormat> made;
    if (FAILED(dwrite->CreateTextFormat(face.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                        size, L"", &made))) {
        return nullptr;
    }
    made->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    made->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    made->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    return made;
}

Text make_text(IDWriteFactory* dwrite, float s) {
    const std::wstring display = family(dwrite, L"Segoe UI Variable Display");
    const std::wstring body = family(dwrite, L"Segoe UI Variable Text");
    Text t;
    t.eyebrow = format(dwrite, body, 13 * s, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.title = format(dwrite, display, 32 * s, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.line = format(dwrite, body, 17 * s, DWRITE_FONT_WEIGHT_NORMAL);
    t.goal = format(dwrite, body, 18 * s, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.big = format(dwrite, display, 54 * s, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.small = format(dwrite, body, 13 * s, DWRITE_FONT_WEIGHT_NORMAL);
    t.button = format(dwrite, body, 16 * s, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.footer = format(dwrite, body, 14 * s, DWRITE_FONT_WEIGHT_NORMAL);
    return t;
}

// Where everything goes, for drawing and for finding what the pointer is over.
struct Layout {
    D2D1_POINT_2F centre{};
    float radius = 0;
    D2D1_RECT_F eyebrow{}, title{}, rule{}, goal{}, close{}, go_on{}, footer{};
};

Layout layout_for(D2D1_SIZE_F size, float s, bool has_goal) {
    const float column = std::min(size.width - 64 * s, 680 * s);
    const float left = (size.width - column) / 2;
    const float right = left + column;
    const float total = (36 + 176 + 28 + 52 + 24 + (has_goal ? 36 : 0) + 40 + 52 + 22 + 22) * s;
    float y = std::max(24 * s, (size.height - total) / 2);

    Layout l;
    l.eyebrow = {left, y, right, y + 22 * s};
    y += 36 * s;
    l.radius = 80 * s;
    l.centre = {size.width / 2, y + 88 * s};
    y += 176 * s + 28 * s;
    l.title = {left, y, right, y + 44 * s};
    y += 52 * s;
    l.rule = {left, y, right, y + 24 * s};
    y += 24 * s;
    if (has_goal) {
        l.goal = {left, y + 8 * s, right, y + 36 * s};
        y += 36 * s;
    }
    y += 40 * s;
    const float width = 250 * s;
    const float gap = 16 * s;
    l.close = {size.width / 2 - gap / 2 - width, y, size.width / 2 - gap / 2, y + 52 * s};
    l.go_on = {size.width / 2 + gap / 2, y, size.width / 2 + gap / 2 + width, y + 52 * s};
    y += 52 * s + 22 * s;
    l.footer = {left, y, right, y + 22 * s};
    return l;
}

void text(ID2D1RenderTarget* rt, IDWriteTextFormat* fmt, const std::wstring& s, const D2D1_RECT_F& r, std::uint32_t rgb,
          float alpha = 1.0f) {
    if (!fmt) return;
    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(rt->CreateSolidColorBrush(colour(rgb, alpha), &brush))) return;
    rt->DrawText(s.c_str(), static_cast<UINT32>(s.size()), fmt, r, brush.Get());
}

void button(ID2D1RenderTarget* rt, const Text& t, const D2D1_RECT_F& r, const std::wstring& label, bool primary,
            bool enabled, bool hover, bool focus, float s) {
    const float round = (r.bottom - r.top) / 2;
    const D2D1_ROUNDED_RECT shape = D2D1::RoundedRect(r, round, round);
    ComPtr<ID2D1SolidColorBrush> brush;
    if (primary) {
        rt->CreateSolidColorBrush(colour(hover ? 0x86ecf0 : kAccent), &brush);
        rt->FillRoundedRectangle(shape, brush.Get());
    } else {
        if (enabled && hover) {
            rt->CreateSolidColorBrush(colour(0xffffff, 0.07f), &brush);
            rt->FillRoundedRectangle(shape, brush.Get());
        }
        rt->CreateSolidColorBrush(colour(kAccent, enabled ? 0.45f : 0.2f), &brush);
        rt->DrawRoundedRectangle(shape, brush.Get(), 1.5f * s);
    }
    if (focus) {
        const D2D1_RECT_F ring{r.left - 4 * s, r.top - 4 * s, r.right + 4 * s, r.bottom + 4 * s};
        rt->CreateSolidColorBrush(colour(kText), &brush);
        rt->DrawRoundedRectangle(D2D1::RoundedRect(ring, round + 4 * s, round + 4 * s), brush.Get(), 2 * s);
    }
    text(rt, t.button.Get(), label, r, primary ? kInk : enabled ? kText : kMuted);
}

std::wstring seconds_left(const PauseView& view, double elapsed) {
    return std::to_wstring(static_cast<int>(std::ceil(std::max(0.0, view.pause_seconds - elapsed))));
}

// hovered/focused: 0 none, 1 close, 2 continue.
void draw(ID2D1RenderTarget* rt, const Text& t, const PauseView& view, double elapsed, int hovered, int focused, float s) {
    const D2D1_SIZE_F size = rt->GetSize();
    const Layout l = layout_for(size, s, !view.goal.empty());
    const bool ready = elapsed >= view.pause_seconds;
    const float progress = view.pause_seconds > 0 ? static_cast<float>(std::clamp(elapsed / view.pause_seconds, 0.0, 1.0)) : 1.0f;

    rt->Clear(colour(kGround));

    // A soft glow behind the ring.
    const D2D1_GRADIENT_STOP stops[] = {{0.0f, colour(kAccent, 0.16f)}, {1.0f, colour(kAccent, 0.0f)}};
    ComPtr<ID2D1GradientStopCollection> collection;
    ComPtr<ID2D1RadialGradientBrush> glow;
    if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &collection)) &&
        SUCCEEDED(rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(l.centre, {}, 300 * s, 300 * s),
                                                collection.Get(), &glow))) {
        rt->FillEllipse(D2D1::Ellipse(l.centre, 300 * s, 300 * s), glow.Get());
    }

    text(rt, t.eyebrow.Get(), L"P A U S E", l.eyebrow, kAccent);

    // The ring: a quiet track, and the countdown filling it clockwise from the top.
    ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(colour(kAccent, 0.16f), &brush);
    rt->DrawEllipse(D2D1::Ellipse(l.centre, l.radius, l.radius), brush.Get(), 8 * s);
    rt->CreateSolidColorBrush(colour(kAccent), &brush);
    if (progress >= 0.999f) {
        rt->DrawEllipse(D2D1::Ellipse(l.centre, l.radius, l.radius), brush.Get(), 8 * s);
    } else if (progress > 0.0f) {
        ComPtr<ID2D1Factory> factory;
        rt->GetFactory(&factory);
        ComPtr<ID2D1PathGeometry> arc;
        ComPtr<ID2D1GeometrySink> sink;
        if (SUCCEEDED(factory->CreatePathGeometry(&arc)) && SUCCEEDED(arc->Open(&sink))) {
            const float angle = progress * 2 * std::numbers::pi_v<float>;
            sink->BeginFigure({l.centre.x, l.centre.y - l.radius}, D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddArc(D2D1::ArcSegment({l.centre.x + l.radius * std::sin(angle), l.centre.y - l.radius * std::cos(angle)},
                                          {l.radius, l.radius}, 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                          progress > 0.5f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close();
            ComPtr<ID2D1StrokeStyle> round;
            factory->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND), nullptr, 0, &round);
            rt->DrawGeometry(arc.Get(), brush.Get(), 8 * s, round.Get());
        }
    }
    const D2D1_RECT_F inside{l.centre.x - l.radius, l.centre.y - l.radius, l.centre.x + l.radius, l.centre.y + l.radius};
    if (ready) {
        text(rt, t.goal.Get(), L"Your call", inside, kText);
    } else {
        text(rt, t.big.Get(), seconds_left(view, elapsed), {inside.left, inside.top, inside.right, l.centre.y + 18 * s}, kText);
        text(rt, t.small.Get(), L"seconds", {inside.left, l.centre.y + 20 * s, inside.right, l.centre.y + 44 * s}, kMuted);
    }

    text(rt, t.title.Get(), L"You opened " + view.program, l.title, kText);
    text(rt, t.line.Get(),
         view.rule_name.empty() ? L"A nudge rule asks you to pause first." : L"“" + view.rule_name + L"” asks you to pause first.",
         l.rule, kMuted);
    if (!view.goal.empty()) text(rt, t.goal.Get(), L"Your goal: " + view.goal, l.goal, kText);

    button(rt, t, l.close, L"Close " + view.program, true, true, hovered == 1, focused == 1, s);
    const std::wstring go_on = ready ? L"Continue for " + std::to_wstring(view.continue_minutes) + L" minutes"
                                     : L"Continue in " + seconds_left(view, elapsed) + L" s";
    button(rt, t, l.go_on, go_on, false, ready, ready && hovered == 2, focused == 2, s);

    text(rt, t.footer.Get(), view.program + L" keeps running behind this screen until you choose.", l.footer, kMuted, 0.8f);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------

struct PauseOverlay::Impl {
    HINSTANCE instance = nullptr;
    Choice choice;
    HWND window = nullptr;
    PauseView view;
    std::chrono::steady_clock::time_point started{};  // when the countdown would have begun
    int hovered = 0;
    int focused = 1;
    float scale = 1;
    ComPtr<ID2D1Factory> d2d;
    ComPtr<IDWriteFactory> dwrite;
    ComPtr<ID2D1HwndRenderTarget> target;
    Text text;

    double elapsed() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    }
    bool ready() const { return elapsed() >= view.pause_seconds; }

    Layout layout() const {
        RECT client{};
        GetClientRect(window, &client);
        return layout_for(D2D1::SizeF(static_cast<float>(client.right), static_cast<float>(client.bottom)), scale,
                          !view.goal.empty());
    }

    int hit(POINT p) const {
        const Layout l = layout();
        const auto inside = [&](const D2D1_RECT_F& r) {
            const auto x = static_cast<float>(p.x);
            const auto y = static_cast<float>(p.y);
            return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
        };
        if (inside(l.close)) return 1;
        if (inside(l.go_on)) return 2;
        return 0;
    }

    void choose(int which) {
        if (which == 2 && !ready()) return;
        if (which != 1 && which != 2) return;
        const PauseView chosen = view;
        choice(chosen, which == 1);
    }

    void paint() {
        if (!target) {
            RECT client{};
            GetClientRect(window, &client);
            const auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), 96.0f, 96.0f);
            const auto hwnd_props = D2D1::HwndRenderTargetProperties(
                window, D2D1::SizeU(static_cast<UINT32>(client.right), static_cast<UINT32>(client.bottom)));
            if (FAILED(d2d->CreateHwndRenderTarget(props, hwnd_props, &target))) return;
            text = make_text(dwrite.Get(), scale);
        }
        target->BeginDraw();
        draw(target.Get(), text, view, elapsed(), hovered, focused, scale);
        if (target->EndDraw() == D2DERR_RECREATE_TARGET) target.Reset();
    }

    LRESULT handle(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                BeginPaint(window, &ps);
                paint();
                EndPaint(window, &ps);
                return 0;
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_TIMER:
                if (ready()) KillTimer(window, kTickTimer);
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            case WM_MOUSEMOVE: {
                const int over = hit({static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))});
                if (over != hovered) {
                    hovered = over;
                    InvalidateRect(window, nullptr, FALSE);
                }
                return 0;
            }
            case WM_SETCURSOR:
                if (hovered == 1 || (hovered == 2 && ready())) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
                break;
            case WM_LBUTTONUP:
                choose(hit({static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))}));
                return 0;
            case WM_KEYDOWN:
                if (wparam == VK_TAB || wparam == VK_LEFT || wparam == VK_RIGHT) {
                    focused = focused == 1 && ready() ? 2 : 1;
                    InvalidateRect(window, nullptr, FALSE);
                } else if (wparam == VK_RETURN || wparam == VK_SPACE) {
                    choose(focused);
                }
                return 0;
            case WM_CLOSE:
                return 0;  // the pause ends with a choice, not Alt+F4
            default:
                break;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    static LRESULT CALLBACK proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!self || !self->window) return DefWindowProcW(hwnd, message, wparam, lparam);
        return self->handle(message, wparam, lparam);
    }
};

PauseOverlay::PauseOverlay(HINSTANCE instance, Choice choice) : impl_(std::make_unique<Impl>()) {
    impl_->instance = instance;
    impl_->choice = std::move(choice);
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, impl_->d2d.GetAddressOf());
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(impl_->dwrite.GetAddressOf()));

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = Impl::proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = L"SirenproofPause";
    RegisterClassExW(&window_class);
}

PauseOverlay::~PauseOverlay() { hide(); }

void PauseOverlay::show(const PauseView& view, std::chrono::seconds waited) {
    if (!impl_->d2d || !impl_->dwrite) return;
    const bool same = impl_->window && impl_->view.id == view.id;
    impl_->view = view;
    if (same) return;
    impl_->started = std::chrono::steady_clock::now() - waited;
    impl_->hovered = 0;
    impl_->focused = 1;

    // Cover the monitor in use, which is where the program just opened.
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY), &monitor);
    const RECT& area = monitor.rcMonitor;
    if (!impl_->window) {
        const std::wstring title = L"Sirenproof: pause before " + view.program;
        impl_->window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, L"SirenproofPause", title.c_str(),
                                        WS_POPUP, area.left, area.top, area.right - area.left, area.bottom - area.top,
                                        nullptr, nullptr, impl_->instance, impl_.get());
        if (!impl_->window) return;
        SetLayeredWindowAttributes(impl_->window, 0, kOpacity, LWA_ALPHA);
    } else {
        SetWindowPos(impl_->window, HWND_TOPMOST, area.left, area.top, area.right - area.left, area.bottom - area.top,
                     SWP_NOACTIVATE);
        impl_->target.Reset();
    }
    impl_->scale = static_cast<float>(GetDpiForWindow(impl_->window)) / 96.0f;
    ShowWindow(impl_->window, SW_SHOW);
    SetForegroundWindow(impl_->window);
    if (!impl_->ready()) SetTimer(impl_->window, kTickTimer, kTickMs, nullptr);
    InvalidateRect(impl_->window, nullptr, FALSE);
}

void PauseOverlay::hide() {
    if (!impl_->window) return;
    HWND window = impl_->window;
    impl_->target.Reset();
    impl_->window = nullptr;
    DestroyWindow(window);
    impl_->view = {};
}

bool PauseOverlay::showing() const { return impl_->window != nullptr; }

const std::string& PauseOverlay::id() const { return impl_->view.id; }

// ---------------------------------------------------------------------------------------------------

bool render_pause_png(const PauseView& view, double elapsed, int width, int height, const std::wstring& file) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool saved = false;
    {
        ComPtr<IWICImagingFactory> wic;
        ComPtr<IWICBitmap> bitmap;
        ComPtr<ID2D1Factory> d2d;
        ComPtr<IDWriteFactory> dwrite;
        ComPtr<ID2D1RenderTarget> target;
        const auto w = static_cast<UINT>(width);
        const auto h = static_cast<UINT>(height);
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) &&
            SUCCEEDED(wic->CreateBitmap(w, h, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap)) &&
            SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d.GetAddressOf())) &&
            SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                          reinterpret_cast<IUnknown**>(dwrite.GetAddressOf()))) &&
            SUCCEEDED(d2d->CreateWicBitmapRenderTarget(
                bitmap.Get(),
                D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                             D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f,
                                             96.0f),
                &target))) {
            const Text t = make_text(dwrite.Get(), 1.0f);
            target->BeginDraw();
            draw(target.Get(), t, view, elapsed, 0, 1, 1.0f);
            ComPtr<IWICStream> stream;
            ComPtr<IWICBitmapEncoder> encoder;
            ComPtr<IWICBitmapFrameEncode> frame;
            WICPixelFormatGUID pixels = GUID_WICPixelFormat32bppPBGRA;
            saved = SUCCEEDED(target->EndDraw()) && SUCCEEDED(wic->CreateStream(&stream)) &&
                    SUCCEEDED(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE)) &&
                    SUCCEEDED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
                    SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) &&
                    SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) &&
                    SUCCEEDED(frame->SetSize(w, h)) && SUCCEEDED(frame->SetPixelFormat(&pixels)) &&
                    SUCCEEDED(frame->WriteSource(bitmap.Get(), nullptr)) && SUCCEEDED(frame->Commit()) &&
                    SUCCEEDED(encoder->Commit());
        }
    }
    if (SUCCEEDED(com)) CoUninitialize();
    return saved;
}

}  // namespace sirenproof::tray

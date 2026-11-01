#include "sirenproof/core/target.hpp"

#include <algorithm>
#include <array>

namespace sirenproof::core {

namespace {

constexpr auto kProtectedDomains = std::to_array<std::string_view>({
    "windowsupdate.com", "update.microsoft.com", "delivery.mp.microsoft.com", "login.live.com",
    "login.microsoftonline.com", "msftconnecttest.com", "msftncsi.com",
});

// Programs that keep Windows, security and accessibility working, plus Sirenproof itself.
constexpr auto kProtectedPrograms = std::to_array<std::string_view>({
    // Windows
    "system", "registry", "smss.exe", "csrss.exe", "wininit.exe", "winlogon.exe", "services.exe",
    "lsass.exe", "lsaiso.exe", "svchost.exe", "explorer.exe", "dwm.exe", "fontdrvhost.exe", "sihost.exe",
    "ctfmon.exe", "conhost.exe", "taskhostw.exe", "runtimebroker.exe", "startmenuexperiencehost.exe",
    "shellexperiencehost.exe", "searchhost.exe", "textinputhost.exe", "logonui.exe", "userinit.exe",
    "audiodg.exe", "spoolsv.exe", "wuauclt.exe", "usoclient.exe", "trustedinstaller.exe",
    // Security
    "msmpeng.exe", "nissrv.exe", "mpcmdrun.exe", "securityhealthservice.exe", "securityhealthsystray.exe",
    // Assistive technology
    "narrator.exe", "magnify.exe", "osk.exe", "atbroker.exe", "utilman.exe", "voiceaccess.exe",
    "nvda.exe", "jfw.exe", "zt.exe",
    // Sirenproof
    "sirenproof.exe", "sirenproof-service.exe", "sirenproof-tray.exe", "sirenproof-ui.exe",
    "sirenproof-host.exe",
});

constexpr auto kProtectedFolders = std::to_array<std::string_view>({
    "c:\\windows\\",
    "c:\\program files\\sirenproof\\",
    "c:\\program files\\nvda\\",
    "c:\\program files (x86)\\nvda\\",
    "c:\\program files\\freedom scientific\\",
    "c:\\program files\\windows defender\\",
});

constexpr auto kProtectedPublishers =
    std::to_array<std::string_view>({"microsoft corporation", "microsoft windows"});

std::optional<std::string> protected_domain_reason(const Domain& host, bool whole_site) {
    for (const std::string_view name : kProtectedDomains) {
        const Domain protected_domain = *Domain::parse(name);
        if (protected_domain.covers(host) || (whole_site && host.covers(protected_domain))) {
            return "it would block " + protected_domain.str() + ", which Windows needs for updates or sign-in";
        }
    }
    return std::nullopt;
}

std::optional<std::string> protected_app_reason(const AppPattern& app) {
    using Kind = AppPattern::Kind;
    switch (app.kind) {
        case Kind::Name:
            if (std::ranges::find(kProtectedPrograms, app.value) != kProtectedPrograms.end()) {
                return app.value + " is part of Windows, security, accessibility or Sirenproof";
            }
            return std::nullopt;
        case Kind::Path:
        case Kind::Folder:
            for (const std::string_view folder : kProtectedFolders) {
                if (app.value.starts_with(folder) || (app.kind == Kind::Folder && folder.starts_with(app.value))) {
                    return "it would block programs in " + std::string(folder);
                }
            }
            if (app.kind == Kind::Path) {
                const std::string_view name = std::string_view(app.value).substr(app.value.rfind('\\') + 1);
                if (std::ranges::find(kProtectedPrograms, name) != kProtectedPrograms.end()) {
                    return std::string(name) + " is part of Windows, security, accessibility or Sirenproof";
                }
            }
            return std::nullopt;
        case Kind::Publisher:
            if (std::ranges::find(kProtectedPublishers, app.value) != kProtectedPublishers.end()) {
                return "it would block parts of Windows; target the program or package instead";
            }
            return std::nullopt;
        case Kind::Package:
            return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace

std::optional<Target> parse_target(std::string_view text) {
    if (text.starts_with("app:")) {
        if (auto app = AppPattern::parse_program(text.substr(4))) return Target{*std::move(app)};
        return std::nullopt;
    }
    if (text.starts_with("publisher:")) {
        if (auto app = AppPattern::parse(AppPattern::Kind::Publisher, text.substr(10))) return Target{*std::move(app)};
        return std::nullopt;
    }
    if (text.starts_with("package:")) {
        if (auto app = AppPattern::parse(AppPattern::Kind::Package, text.substr(8))) return Target{*std::move(app)};
        return std::nullopt;
    }
    if (text.find('/') != std::string_view::npos) {
        if (auto page = PagePattern::parse(text)) return Target{*std::move(page)};
        return std::nullopt;
    }
    if (auto domain = Domain::parse(text)) return Target{*std::move(domain)};
    return std::nullopt;
}

std::string to_string(const Target& target) {
    struct Visitor {
        std::string operator()(const Domain& d) const { return d.str(); }
        std::string operator()(const PagePattern& p) const { return p.to_string(); }
        std::string operator()(const AppPattern& a) const {
            switch (a.kind) {
                case AppPattern::Kind::Publisher: return "publisher:" + a.value;
                case AppPattern::Kind::Package: return "package:" + a.value;
                default: return "app:" + a.value;
            }
        }
    };
    return std::visit(Visitor{}, target);
}

bool is_web_target(const Target& target) { return !std::holds_alternative<AppPattern>(target); }

std::optional<std::string> protected_reason(const Target& target) {
    if (const auto* domain = std::get_if<Domain>(&target)) return protected_domain_reason(*domain, true);
    if (const auto* page = std::get_if<PagePattern>(&target)) return protected_domain_reason(page->host, false);
    return protected_app_reason(std::get<AppPattern>(target));
}

}  // namespace sirenproof::core

#include "sirenproof/core/app.hpp"

#include <algorithm>

namespace sirenproof::core {

namespace {

constexpr char to_lower_ascii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string lower(std::string_view text) {
    std::string out(text.size(), '\0');
    std::ranges::transform(text, out.begin(), to_lower_ascii);
    return out;
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}

bool has_control_char(std::string_view text) {
    return std::ranges::any_of(text, [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u < 0x20 || u == 0x7f;
    });
}

// Characters Windows never allows in a file name (besides the separators).
bool has_invalid_name_char(std::string_view text) {
    return text.find_first_of("<>:\"|?*") != std::string_view::npos;
}

bool is_absolute_drive_path(std::string_view p) {
    return p.size() >= 3 && ((p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':' && p[2] == '\\';
}

std::string_view file_name(std::string_view path) {
    const std::size_t slash = path.rfind('\\');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

}  // namespace

std::string normalise_app_path(std::string_view path) {
    if (path.starts_with("\\\\?\\") || path.starts_with("//?/")) path.remove_prefix(4);
    std::string out;
    out.reserve(path.size());
    for (char c : path) {
        c = c == '/' ? '\\' : to_lower_ascii(c);
        if (c == '\\' && !out.empty() && out.back() == '\\') continue;
        out.push_back(c);
    }
    return out;
}

std::optional<AppPattern> AppPattern::parse(Kind kind, std::string_view spec) {
    spec = trim(spec);
    if (spec.empty() || has_control_char(spec)) return std::nullopt;

    switch (kind) {
        case Kind::Name: {
            if (spec.find_first_of("\\/") != std::string_view::npos || has_invalid_name_char(spec)) {
                return std::nullopt;
            }
            std::string name = lower(spec);
            if (name == "." || name == "..") return std::nullopt;
            if (name.find('.') == std::string::npos) name += ".exe";
            return AppPattern{kind, std::move(name)};
        }
        case Kind::Path:
        case Kind::Folder: {
            std::string path = normalise_app_path(spec);
            if (!is_absolute_drive_path(path) || has_invalid_name_char(std::string_view(path).substr(2))) {
                return std::nullopt;
            }
            if (path.find("\\..\\") != std::string::npos || path.ends_with("\\..") ||
                path.find("\\.\\") != std::string::npos) {
                return std::nullopt;
            }
            const bool is_folder = path.back() == '\\';
            if (is_folder != (kind == Kind::Folder)) return std::nullopt;
            return AppPattern{kind, std::move(path)};
        }
        case Kind::Publisher:
            return AppPattern{kind, lower(spec)};
        case Kind::Package: {
            const bool valid = std::ranges::all_of(spec, [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                       c == '_' || c == '-';
            });
            if (!valid) return std::nullopt;
            return AppPattern{kind, lower(spec)};
        }
    }
    return std::nullopt;
}

std::optional<AppPattern> AppPattern::parse_program(std::string_view spec) {
    const std::string normalised = normalise_app_path(trim(spec));
    if (normalised.find('\\') == std::string::npos && normalised.find(':') == std::string::npos) {
        return parse(Kind::Name, spec);
    }
    return parse(normalised.ends_with('\\') ? Kind::Folder : Kind::Path, spec);
}

bool AppPattern::matches(const AppIdentity& app) const {
    switch (kind) {
        case Kind::Name: {
            const std::string path = normalise_app_path(app.path);
            return file_name(path) == value || lower(app.original_filename) == value;
        }
        case Kind::Path:
            return normalise_app_path(app.path) == value;
        case Kind::Folder:
            return normalise_app_path(app.path).starts_with(value);
        case Kind::Publisher:
            return !app.publisher.empty() && lower(trim(app.publisher)) == value;
        case Kind::Package:
            return !app.package_family.empty() && lower(app.package_family) == value;
    }
    return false;
}

}  // namespace sirenproof::core

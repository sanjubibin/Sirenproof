#include "sirenproof/core/url.hpp"

#include <algorithm>

namespace sirenproof::core {

namespace {

constexpr char to_lower_ascii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

constexpr bool is_unreserved(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
           c == '.' || c == '_' || c == '~';
}

constexpr int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool has_forbidden_char(std::string_view text) {
    return std::ranges::any_of(text, [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u <= 0x20 || u == 0x7f || c == '#' || c == '\\';
    });
}

std::vector<QueryParam> parse_query(std::string_view query) {
    std::vector<QueryParam> params;
    while (!query.empty()) {
        const std::size_t amp = query.find('&');
        const std::string_view part = query.substr(0, amp);
        query = amp == std::string_view::npos ? std::string_view{} : query.substr(amp + 1);
        if (part.empty()) continue;
        const std::size_t eq = part.find('=');
        if (eq == std::string_view::npos) {
            params.push_back({std::string(part), std::nullopt});
        } else {
            params.push_back({std::string(part.substr(0, eq)), std::string(part.substr(eq + 1))});
        }
    }
    return params;
}

}  // namespace

std::string normalise_path(std::string_view path) {
    std::string out;
    out.reserve(path.size() + 1);
    for (std::size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        if (c == '%' && i + 2 < path.size()) {
            const int hi = hex_value(path[i + 1]);
            const int lo = hex_value(path[i + 2]);
            if (hi >= 0 && lo >= 0) {
                const char decoded = static_cast<char>(hi * 16 + lo);
                if (is_unreserved(decoded)) {
                    c = decoded;
                    i += 2;
                }
            }
        }
        c = to_lower_ascii(c);
        if (c == '/' && !out.empty() && out.back() == '/') continue;
        out.push_back(c);
    }
    if (out.empty() || out.front() != '/') out.insert(out.begin(), '/');
    return out;
}

std::optional<Url> Url::parse(std::string_view text) {
    std::string_view rest = text;

    const std::size_t scheme_end = rest.find("://");
    if (scheme_end != std::string_view::npos) {
        std::string scheme(rest.substr(0, scheme_end));
        std::ranges::transform(scheme, scheme.begin(), to_lower_ascii);
        if (scheme != "http" && scheme != "https") return std::nullopt;
        rest.remove_prefix(scheme_end + 3);
    }

    const std::size_t authority_end = rest.find_first_of("/?#");
    std::string_view authority = rest.substr(0, authority_end);
    rest = authority_end == std::string_view::npos ? std::string_view{} : rest.substr(authority_end);

    if (const std::size_t at = authority.rfind('@'); at != std::string_view::npos) {
        authority.remove_prefix(at + 1);
    }
    if (const std::size_t colon = authority.rfind(':'); colon != std::string_view::npos) {
        const std::string_view port = authority.substr(colon + 1);
        // A colon followed by anything but a port number means a scheme like "about:blank".
        if (port.empty() || !std::ranges::all_of(port, [](char c) { return c >= '0' && c <= '9'; })) {
            return std::nullopt;
        }
        authority = authority.substr(0, colon);
    }

    auto host = Domain::parse(authority);
    if (!host) return std::nullopt;

    const std::size_t fragment = rest.find('#');
    if (fragment != std::string_view::npos) rest = rest.substr(0, fragment);
    const std::size_t question = rest.find('?');
    const std::string_view path = rest.substr(0, question);
    const std::string_view query =
        question == std::string_view::npos ? std::string_view{} : rest.substr(question + 1);

    return Url{*std::move(host), normalise_path(path), std::string(query)};
}

std::optional<PagePattern> PagePattern::parse(std::string_view text) {
    if (has_forbidden_char(text) || text.find("://") != std::string_view::npos) return std::nullopt;

    const std::size_t slash = text.find('/');
    if (slash == std::string_view::npos) return std::nullopt;  // no path: that's a Domain target

    auto host = Domain::parse(text.substr(0, slash));
    if (!host) return std::nullopt;

    const std::string_view rest = text.substr(slash);
    const std::size_t question = rest.find('?');
    std::string path = normalise_path(rest.substr(0, question));
    if (path.size() > 1 && path.back() == '/') path.pop_back();  // "/shorts/" means "/shorts"

    std::vector<QueryParam> query;
    if (question != std::string_view::npos) {
        query = parse_query(rest.substr(question + 1));
        if (query.empty()) return std::nullopt;
        if (std::ranges::any_of(query, [](const QueryParam& p) { return p.key.empty(); })) return std::nullopt;
    }
    return PagePattern{*std::move(host), std::move(path), std::move(query)};
}

bool PagePattern::matches(const Url& url) const {
    if (!host.covers(url.host)) return false;

    if (path == "/") {
        if (url.path != "/") return false;
    } else if (url.path != path) {
        if (!url.path.starts_with(path) || url.path[path.size()] != '/') return false;
    }

    if (query.empty()) return true;
    const std::vector<QueryParam> present = parse_query(url.query);
    return std::ranges::all_of(query, [&](const QueryParam& wanted) {
        return std::ranges::any_of(present, [&](const QueryParam& p) {
            return p.key == wanted.key && (!wanted.value || p.value == wanted.value);
        });
    });
}

std::string PagePattern::to_string() const {
    std::string text = host.str() + path;
    for (std::size_t i = 0; i < query.size(); ++i) {
        text += i == 0 ? '?' : '&';
        text += query[i].key;
        if (query[i].value) text += '=' + *query[i].value;
    }
    return text;
}

}  // namespace sirenproof::core

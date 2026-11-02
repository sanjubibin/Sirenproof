#include "sirenproof/core/hosts.hpp"

#include <algorithm>

namespace sirenproof::core {

namespace {

constexpr std::string_view kPrefixes[] = {"www.", "m."};

bool is_our_entry(std::string_view line) { return line.starts_with("0.0.0.0 ") || line.starts_with(":: "); }

// Splits into lines without their terminators; `ends_with_newline` reports whether the text did.
std::vector<std::string_view> split_lines(std::string_view text, bool& ends_with_newline) {
    std::vector<std::string_view> lines;
    ends_with_newline = text.empty() || text.back() == '\n';
    while (!text.empty()) {
        const std::size_t newline = text.find('\n');
        std::string_view line = text.substr(0, newline);
        if (line.ends_with('\r')) line.remove_suffix(1);
        lines.push_back(line);
        if (newline == std::string_view::npos) break;
        text.remove_prefix(newline + 1);
    }
    return lines;
}

}  // namespace

std::vector<std::string> hosts_names_for(const Domain& site) {
    std::vector<std::string> names{site.str()};
    for (const std::string_view prefix : kPrefixes) {
        if (site.str().starts_with(prefix)) continue;
        if (auto prefixed = Domain::parse(std::string(prefix) + site.str())) names.push_back(prefixed->str());
    }
    return names;
}

std::string apply_hosts_section(std::string_view existing, const std::vector<Domain>& sites) {
    // The file's own line ending; Windows' CRLF if it has none yet.
    const bool lf_only = existing.find('\n') != std::string_view::npos && existing.find("\r\n") == std::string_view::npos;
    const std::string_view eol = lf_only ? "\n" : "\r\n";

    bool ends_with_newline = true;
    const std::vector<std::string_view> lines = split_lines(existing, ends_with_newline);

    // Keep every line outside our section(s).
    std::vector<std::string_view> kept;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (!lines[i].starts_with("# BEGIN Sirenproof")) {
            kept.push_back(lines[i]);
            continue;
        }
        std::size_t end = i + 1;
        while (end < lines.size() && lines[end] != kHostsEnd) ++end;
        if (end < lines.size()) {
            i = end;  // skip through the END line
        } else {
            while (i + 1 < lines.size() && is_our_entry(lines[i + 1])) ++i;  // damaged: only our own lines
        }
    }

    std::string out;
    for (std::size_t i = 0; i < kept.size(); ++i) {
        out += kept[i];
        if (i + 1 < kept.size() || ends_with_newline) out += eol;
    }

    if (sites.empty()) return out;

    if (!out.empty() && !out.ends_with('\n')) out += eol;
    out += kHostsBegin;
    out += eol;
    std::vector<std::string> names;
    for (const Domain& site : sites) {
        for (std::string& name : hosts_names_for(site)) names.push_back(std::move(name));
    }
    std::ranges::sort(names);
    const auto duplicates = std::ranges::unique(names);
    names.erase(duplicates.begin(), duplicates.end());
    for (const std::string& name : names) {
        out += "0.0.0.0 " + name;
        out += eol;
        out += ":: " + name;
        out += eol;
    }
    out += kHostsEnd;
    out += eol;
    return out;
}

}  // namespace sirenproof::core

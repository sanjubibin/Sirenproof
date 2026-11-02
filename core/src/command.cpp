#include "sirenproof/core/command.hpp"

#include <algorithm>
#include <iterator>

#include "sirenproof/core/target.hpp"

namespace sirenproof::core {

namespace {

constexpr std::string_view kVideo[] = {"youtube.com/", "youtube.com/feed", "youtube.com/shorts"};
constexpr std::string_view kSocial[] = {"reddit.com", "x.com", "instagram.com"};
constexpr std::string_view kGames[] = {"app:steam.exe", "app:discord.exe"};

constexpr ReadyList kLists[] = {
    {"video", "Video", "YouTube home, feed and Shorts", "Blocks YouTube's home page, feed and Shorts. Videos still play.",
     kVideo},
    {"social", "Social", "Reddit, X, Instagram", "Blocks reddit.com, x.com and instagram.com.", kSocial},
    {"games", "Games", "Steam, Discord", "Closes Steam and Discord. Anything already open gets 10 seconds to save.",
     kGames},
};

struct Name {
    std::string_view name;
    std::span<const std::string_view> targets;
};

constexpr std::string_view kYoutube[] = {"youtube.com"};
constexpr std::string_view kReddit[] = {"reddit.com"};
constexpr std::string_view kX[] = {"x.com", "twitter.com"};
constexpr std::string_view kInstagram[] = {"instagram.com"};
constexpr std::string_view kFacebook[] = {"facebook.com"};
constexpr std::string_view kTiktok[] = {"tiktok.com"};
constexpr std::string_view kNetflix[] = {"netflix.com"};
constexpr std::string_view kTwitch[] = {"twitch.tv"};
constexpr std::string_view kSteam[] = {"app:steam.exe"};
constexpr std::string_view kDiscord[] = {"app:discord.exe", "discord.com"};

constexpr Name kNames[] = {
    {"youtube", kYoutube},     {"reddit", kReddit},   {"x", kX},           {"twitter", kX},
    {"instagram", kInstagram}, {"facebook", kFacebook}, {"tiktok", kTiktok}, {"netflix", kNetflix},
    {"twitch", kTwitch},       {"steam", kSteam},     {"discord", kDiscord},
};

std::string lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    return out;
}

bool is_space(char c) { return c == ' ' || c == '\t'; }

std::vector<std::string_view> split_words(std::string_view text) {
    std::vector<std::string_view> words;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && is_space(text[i])) ++i;
        const std::size_t start = i;
        while (i < text.size() && !is_space(text[i])) ++i;
        if (i > start) words.push_back(text.substr(start, i - start));
    }
    return words;
}

std::unexpected<CommandError> fail(std::string message) { return std::unexpected(CommandError{std::move(message), true}); }

void add_once(std::vector<std::string>& list, std::string item) {
    if (std::ranges::find(list, item) == list.end()) list.push_back(std::move(item));
}

}  // namespace

std::span<const ReadyList> ready_lists() { return kLists; }

const ReadyList* find_ready_list(std::string_view id) {
    const auto* it = std::ranges::find(kLists, id, &ReadyList::id);
    return it == std::end(kLists) ? nullptr : it;
}

std::span<const std::string_view> named_targets(std::string_view name) {
    const auto* it = std::ranges::find(kNames, name, &Name::name);
    return it == std::end(kNames) ? std::span<const std::string_view>{} : it->targets;
}

std::expected<Command, CommandError> parse_command(std::string_view text) {
    const std::vector<std::string_view> words = split_words(text);
    if (words.empty()) return std::unexpected(CommandError{"type a command, e.g. lock youtube 2h", false});
    const std::string verb = lower(words[0]);
    const std::span<const std::string_view> rest(std::next(words.begin()), words.end());

    Command command;
    if (verb == "now" || verb == "status" || verb == "unlock" || verb == "rules" || verb == "lockdown" ||
        (verb == "lock" && rest.empty())) {
        command.kind = Command::Kind::Go;
        command.screen = verb == "rules" || verb == "lock" || verb == "lockdown" ? verb : "now";
        return command;
    }
    if (verb == "break") {
        // How long a break lasts is a setting, so a length is accepted but not needed.
        if (rest.size() > 1 || (rest.size() == 1 && !parse_duration(rest[0]))) {
            return fail("break takes nothing after it");
        }
        command.kind = Command::Kind::Break;
        return command;
    }
    if (verb == "check" || verb == "test") {
        if (rest.empty()) return fail(verb + " needs a web address or a program, e.g. " + verb + " youtube.com/shorts");
        // Everything after the verb, as typed, so a program path with spaces survives.
        std::string_view subject = text.substr(static_cast<std::size_t>(rest.front().data() - text.data()));
        while (!subject.empty() && is_space(subject.back())) subject.remove_suffix(1);
        command.kind = Command::Kind::Check;
        command.subject = std::string(subject);
        return command;
    }
    if (verb != "block" && verb != "nudge" && verb != "lock") {
        return std::unexpected(CommandError{"there's no command called " + std::string(words[0]), false});
    }

    command.kind = verb == "lock" ? Command::Kind::Lock : Command::Kind::Block;
    command.strength = verb == "lock" ? Strength::Lock : verb == "nudge" ? Strength::Nudge : Strength::Block;
    for (std::size_t i = 0; i < rest.size(); ++i) {
        const std::string_view word = rest[i];
        const std::string low = lower(word);
        const auto next = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= rest.size()) return std::nullopt;
            return rest[++i];
        };
        std::optional<std::string_view> length_text;

        if (low == "--for") {
            length_text = next();
            if (!length_text) return fail("--for needs a length, e.g. --for 25m");
        } else if (low == "--lock") {
            if (command.strength == Strength::Nudge) return fail("a lock can't also be a nudge");
            command.kind = Command::Kind::Lock;
            command.strength = Strength::Lock;
        } else if (low == "--nudge") {
            if (command.kind == Command::Kind::Lock) return fail("a lock can't also be a nudge");
            command.strength = Strength::Nudge;
        } else if (low == "--unlock") {
            const auto method = next();
            const std::string m = method ? lower(*method) : std::string{};
            if (m.starts_with("wait")) command.unlock = "wait";
            else if (m.starts_with("type")) command.unlock = "type";
            else if (m == "friend") command.unlock = "friend";
            else if (m == "none") command.unlock = "none";
            else return fail("--unlock expects wait, type, friend or none");
        } else if (low.starts_with("--")) {
            return fail("there's no option called " + std::string(word));
        } else if (parse_duration(word)) {
            length_text = word;
        } else if (find_ready_list(low)) {
            add_once(command.lists, low);
        } else if (const auto named = named_targets(low); !named.empty()) {
            for (const std::string_view t : named) add_once(command.targets, std::string(t));
        } else if (const auto target = parse_target(word)) {
            add_once(command.targets, to_string(*target));
        } else {
            return fail(std::string(word) + " isn't a site, page or app; try reddit.com, youtube.com/shorts or app:steam.exe");
        }

        if (length_text) {
            const auto length = parse_duration(*length_text);
            if (!length) return fail(std::string(*length_text) + " isn't a length; try 25m, 2h or 1h30m");
            if (command.length) return fail("give one length, not two");
            command.length = length;
        }
    }

    if (!command.unlock.empty() && command.kind != Command::Kind::Lock) return fail("only a lock has an unlock method");
    if (command.kind == Command::Kind::Lock && !command.length) return fail("a lock needs a length, e.g. lock youtube 2h");
    if (command.kind == Command::Kind::Block && command.lists.empty() && command.targets.empty()) {
        return fail(verb + " needs something to block, e.g. " + verb + " reddit.com 25m");
    }
    return command;
}

}  // namespace sirenproof::core

#include "game/save.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <span>
#include <vector>
#include <iterator>
#include <limits>
#include <system_error>

namespace heroes::game {

namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

// 2 added "mods" (add-lua-scripting). Version 1 saves still load, with no mods.
constexpr int kVersion = 2;
constexpr int kOldestVersion = 1;
constexpr std::uintmax_t kMaxSaveBytes = 1 << 20; // a real save is a few KB

// Thrown by the field readers; caught at the loadGame boundary and turned into Corrupt.
struct BadSave {};

[[noreturn]] void bad() { throw BadSave{}; }

const Json& field(const Json& obj, const char* key) {
    if (!obj.is_object())
        bad();
    const auto it = obj.find(key);
    if (it == obj.end())
        bad();
    return *it;
}

std::int64_t integer(const Json& obj, const char* key, std::int64_t lo, std::int64_t hi) {
    const Json& v = field(obj, key);
    if (!v.is_number_integer())
        bad();
    const auto n = v.get<std::int64_t>();
    if (v.is_number_unsigned() &&
        v.get<std::uint64_t>() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        bad();
    if (n < lo || n > hi)
        bad();
    return n;
}

int smallInt(const Json& obj, const char* key, int lo, int hi) {
    return static_cast<int>(integer(obj, key, lo, hi));
}

std::string text(const Json& obj, const char* key) {
    const Json& v = field(obj, key);
    if (!v.is_string())
        bad();
    return v.get<std::string>();
}

std::string hex64(std::uint64_t v) { return std::format("0x{:016x}", v); }

std::uint64_t parseHex64(const Json& obj, const char* key) {
    const std::string s = text(obj, key);
    if (s.size() != 18 || s[0] != '0' || s[1] != 'x')
        bad();
    std::uint64_t value = 0;
    const auto [end, ec] = std::from_chars(s.data() + 2, s.data() + s.size(), value, 16);
    if (ec != std::errc{} || end != s.data() + s.size())
        bad();
    return value;
}

Json equippedToJson(const std::optional<Equipped>& e) {
    if (!e)
        return nullptr;
    return Json{{"itemId", e->itemId}, {"upgradeLevel", e->upgradeLevel}};
}

std::optional<Equipped> equippedFromJson(const Json& obj, const char* key, ItemKind kind,
                                         const Content& content) {
    const Json& v = field(obj, key);
    if (v.is_null())
        return std::nullopt;
    Equipped e{text(v, "itemId"), smallInt(v, "upgradeLevel", 1, 10)};
    const ItemDef* item = content.findItem(e.itemId);
    if (!item || item->kind != kind)
        bad();
    return e;
}

Json inventoryToJson(const Inventory& inv) {
    Json slots = Json::array();
    for (const Inventory::Slot& slot : inv.slots())
        slots.push_back(slot ? Json::array({slot->first, slot->second}) : Json(nullptr));
    return slots;
}

Inventory inventoryFromJson(const Json& v, const Content& content) {
    if (!v.is_array() || v.size() != static_cast<std::size_t>(Inventory::kSlots))
        bad();
    std::array<Inventory::Slot, Inventory::kSlots> slots;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (v[i].is_null())
            continue;
        const Json& entry = v[i];
        if (!entry.is_array() || entry.size() != 2 || !entry[0].is_string() ||
            !entry[1].is_number_integer())
            bad();
        const auto id = entry[0].get<std::string>();
        const auto count = entry[1].get<std::int64_t>();
        if (!content.findItem(id) || count < 1 || count > std::numeric_limits<int>::max())
            bad();
        const bool duplicate = std::ranges::any_of(
            slots, [&](const Inventory::Slot& s) { return s && s->first == id; });
        if (duplicate)
            bad();
        slots[i] = std::pair{id, static_cast<int>(count)};
    }
    return Inventory::fromSlots(slots);
}

template <class Lookup>
std::set<std::string> idSet(const Json& obj, const char* key, Lookup known) {
    const Json& v = field(obj, key);
    if (!v.is_array())
        bad();
    std::set<std::string> out;
    for (const Json& e : v) {
        if (!e.is_string() || !known(e.get<std::string>()))
            bad();
        out.insert(e.get<std::string>());
    }
    return out;
}

Json toJson(const GameState& state, std::span<const ModRef> mods) {
    const Hero& h = state.hero;
    Json hero{{"name", h.name},
              {"pathId", h.pathId},
              {"level", h.level},
              {"expToNext", h.expToNext},
              {"exp", h.exp},
              {"gold", h.gold},
              {"hp", h.hp},
              {"mp", h.mp},
              {"maxHp", h.maxHp},
              {"maxMp", h.maxMp},
              {"atk", h.atk},
              {"def", h.def},
              {"life", h.life},
              {"weapon", equippedToJson(h.weapon)},
              {"armor", equippedToJson(h.armor)},
              {"bag", inventoryToJson(h.bag)},
              {"bank", h.bank ? inventoryToJson(*h.bank) : Json(nullptr)},
              {"learnedSkills", h.learnedSkills},
              {"completedMissions", h.completedMissions},
              {"forgeLevel", h.forgeLevel}};
    const Rng::State rng = state.rng.state();
    Json modList = Json::array();
    for (const ModRef& mod : mods)
        modList.push_back({{"name", mod.name}, {"version", mod.version}});
    return Json{{"version", kVersion},
                {"mods", std::move(modList)},
                {"seed", hex64(state.seed)},
                {"rng", {{"state", hex64(rng.state)}, {"inc", hex64(rng.inc)}}},
                {"hero", std::move(hero)}};
}

GameState fromJson(const Json& root, const Content& content) {
    const std::uint64_t seed = parseHex64(root, "seed");
    const Json& rngJson = field(root, "rng");
    const Rng::State rngState{parseHex64(rngJson, "state"), parseHex64(rngJson, "inc")};
    if ((rngState.inc & 1) == 0) // PCG's increment is always odd
        bad();

    const Json& h = field(root, "hero");
    Hero hero;
    hero.name = text(h, "name");
    const auto validName = validateHeroName(hero.name);
    if (!validName || *validName != hero.name)
        bad();
    hero.pathId = text(h, "pathId");
    if (!content.findPath(hero.pathId))
        bad();

    // Stats share the content cap so battle math can't overflow; currencies get far more room
    // than play can reach. Anything larger is a hand edit and is refused, not trusted.
    hero.level = smallInt(h, "level", 1, kMaxStat);
    hero.expToNext = integer(h, "expToNext", 1, kMaxCurrency);
    hero.exp = integer(h, "exp", 0, kMaxCurrency);
    hero.gold = integer(h, "gold", 0, kMaxCurrency);
    hero.maxHp = smallInt(h, "maxHp", 1, kMaxStat);
    hero.maxMp = smallInt(h, "maxMp", 0, kMaxStat);
    hero.hp = smallInt(h, "hp", 0, hero.maxHp);
    hero.mp = smallInt(h, "mp", 0, hero.maxMp);
    hero.atk = smallInt(h, "atk", 0, kMaxStat);
    hero.def = smallInt(h, "def", 0, kMaxStat);
    hero.life = smallInt(h, "life", 0, kMaxStat);
    hero.forgeLevel = smallInt(h, "forgeLevel", 1, 10);
    hero.weapon = equippedFromJson(h, "weapon", ItemKind::Weapon, content);
    hero.armor = equippedFromJson(h, "armor", ItemKind::Armor, content);
    hero.bag = inventoryFromJson(field(h, "bag"), content);
    if (const Json& bank = field(h, "bank"); !bank.is_null())
        hero.bank = inventoryFromJson(bank, content);
    hero.learnedSkills = idSet(h, "learnedSkills", [&](const std::string& id) {
        return content.findSkill(id) != nullptr;
    });
    hero.completedMissions = idSet(h, "completedMissions", [&](const std::string& id) {
        return content.findMission(id) != nullptr;
    });
    return GameState{std::move(hero), Rng::fromState(rngState), seed};
}

} // namespace

std::expected<void, SaveError> saveGame(const GameState& state, const fs::path& file,
                                        std::span<const ModRef> mods) {
    const std::string body = toJson(state, mods).dump(2);
    fs::path tmp = file;
    tmp += ".tmp";

    std::error_code ec;
    if (file.has_parent_path()) {
        fs::create_directories(file.parent_path(), ec);
        if (ec)
            return std::unexpected(SaveError::WriteFailed);
    }
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << body;
        out.flush();
        if (!out) {
            out.close();
            fs::remove(tmp, ec);
            return std::unexpected(SaveError::WriteFailed);
        }
    }
    // ponytail: no fsync before the rename; a power cut (not a crash) could lose the new save.
    fs::rename(tmp, file, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return std::unexpected(SaveError::WriteFailed);
    }
    return {};
}

namespace {

// Reads and parses a save, checking its size and version. Never modifies the file.
std::expected<Json, SaveError> readRoot(const fs::path& file) {
    std::error_code ec;
    if (!fs::exists(file, ec))
        return std::unexpected(SaveError::NotFound);
    const auto size = fs::file_size(file, ec);
    if (ec)
        return std::unexpected(SaveError::Unreadable);
    if (size > kMaxSaveBytes)
        return std::unexpected(SaveError::Corrupt);

    std::ifstream in(file, std::ios::binary);
    if (!in)
        return std::unexpected(SaveError::Unreadable);
    const std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad())
        return std::unexpected(SaveError::Unreadable);

    try {
        Json root = Json::parse(body);
        const Json& version = field(root, "version");
        if (!version.is_number_integer())
            return std::unexpected(SaveError::Corrupt);
        if (version.get<std::int64_t>() > kVersion)
            return std::unexpected(SaveError::NewerVersion);
        if (version.get<std::int64_t>() < kOldestVersion)
            return std::unexpected(SaveError::Corrupt);
        return root;
    } catch (const BadSave&) {
        return std::unexpected(SaveError::Corrupt);
    } catch (const Json::exception&) {
        return std::unexpected(SaveError::Corrupt);
    }
}

// The "mods" list of a version 2 save; none for version 1.
std::vector<ModRef> modsFromJson(const Json& root) {
    if (root.at("version").get<std::int64_t>() < 2)
        return {};
    const Json& list = field(root, "mods");
    if (!list.is_array() || list.size() > 1000)
        bad();
    std::vector<ModRef> mods;
    for (const Json& mod : list)
        mods.push_back({text(mod, "name"), text(mod, "version")});
    return mods;
}

} // namespace

std::expected<std::vector<ModRef>, SaveError> readSaveMods(const fs::path& file) {
    auto root = readRoot(file);
    if (!root)
        return std::unexpected(root.error());
    try {
        return modsFromJson(*root);
    } catch (const BadSave&) {
        return std::unexpected(SaveError::Corrupt);
    } catch (const Json::exception&) {
        return std::unexpected(SaveError::Corrupt);
    }
}

std::expected<GameState, SaveError> loadGame(const fs::path& file, const Content& content) {
    auto root = readRoot(file);
    if (!root)
        return std::unexpected(root.error());
    try {
        (void)modsFromJson(*root); // checked here too, so a bad list can't load
        return fromJson(*root, content);
    } catch (const BadSave&) {
        return std::unexpected(SaveError::Corrupt);
    } catch (const Json::exception&) {
        return std::unexpected(SaveError::Corrupt);
    }
}

} // namespace heroes::game

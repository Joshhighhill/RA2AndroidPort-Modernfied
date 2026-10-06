// P7 Save / Load [RE: docs/re/saveload.md; spec 2026-10-06-saveload-design.md].
#include "doctest.h"

#include <chrono>
#include <fstream>
#include <regex>
#include <sstream>
#include <set>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "../engine/save_fields.hpp"
#include "../engine/savegame.hpp"
#include "asset_fixture.hpp"
#include "ra2assets/ini.hpp"

using namespace ra2engine;
using namespace ra2engine::save;

// ===========================================================================
// Archive core
// ===========================================================================

namespace {
struct Toy {
    int a = 0;
    double b = 0.0;
    std::string s;
    std::vector<int> v;
    bool flags[3] = {false, false, false};
    std::map<std::string, int> m;
    long l = 0;
    std::uint8_t u8 = 0;
};
}  // namespace

namespace ra2engine {
namespace save {
template <class Ar> void fields(Ar& ar, Toy& t) {
    io(ar, t.a); io(ar, t.b); io(ar, t.s); io(ar, t.v); io(ar, t.flags);
    io(ar, t.m); io(ar, t.l); io(ar, t.u8);
}
}  // namespace save
}  // namespace ra2engine

TEST_CASE("saveload: archive round-trips scalars, strings, containers bit-exactly") {
    Toy t;
    t.a = -123456;
    t.b = 0.1 + 0.2;          // a non-representable sum: bit-exact or bust
    t.s = std::string("desc\0x", 6);
    t.v = {1, -2, 3};
    t.flags[1] = true;
    t.m = {{"GI", 4}, {"E2", -9}};
    t.l = -7;
    t.u8 = 250;
    std::vector<std::uint8_t> b = to_bytes(t);
    Toy u;
    REQUIRE(from_bytes(b, u));
    CHECK(u.a == t.a);
    CHECK(std::memcmp(&u.b, &t.b, sizeof(double)) == 0);
    CHECK(u.s == t.s);
    CHECK(u.v == t.v);
    CHECK(u.flags[1]);
    CHECK_FALSE(u.flags[0]);
    CHECK(u.m == t.m);
    CHECK(u.l == -7);
    CHECK(u.u8 == 250);
    CHECK(to_bytes(u) == b);
}

TEST_CASE("saveload: truncated or corrupt input fails cleanly") {
    Toy t;
    t.v.assign(100, 5);
    std::vector<std::uint8_t> b = to_bytes(t);
    for (std::size_t cut : {std::size_t(0), std::size_t(3), b.size() / 2, b.size() - 1}) {
        std::vector<std::uint8_t> c(b.begin(), b.begin() + static_cast<long>(cut));
        Toy u;
        CHECK_FALSE(from_bytes(c, u));
    }
    std::vector<std::uint8_t> huge = b;
    huge[8 + 8 + 8] = 0xFF;   // the string's count byte region: absurd lengths refuse
    Toy u;
    (void)from_bytes(huge, u);  // must not crash / allocate wildly
    CHECK(aggregate_arity<Toy>() == visited_fields<Toy>());
}

// ===========================================================================
// Field-coverage tripwires: adding a member to any of these structs without
// adding it to its visitor in engine/save_fields.hpp FAILS here.
// ===========================================================================

#define CHECK_ARITY(T)                                                              \
    do {                                                                            \
        INFO("struct " #T ": add the new member to its visitor in save_fields.hpp " \
             "and bump save::kFormatVersion");                                      \
        CHECK(aggregate_arity<T>() == visited_fields<T>());                         \
    } while (0)

TEST_CASE("saveload: every serialized aggregate's visitor covers all its members") {
    CHECK_ARITY(Unit);
    CHECK_ARITY(Building);
    CHECK_ARITY(BuildingTypeDef);
    CHECK_ARITY(UnitCombat);
    CHECK_ARITY(UnitTypeDef);
    CHECK_ARITY(ProjectileSpec);
    CHECK_ARITY(ProjectileRules);
    CHECK_ARITY(ProjRng);
    CHECK_ARITY(OreRng);
    CHECK_ARITY(Projectile);
    CHECK_ARITY(ProjectileEvent);
    CHECK_ARITY(CargoEvent);
    CHECK_ARITY(Cellxy);
    CHECK_ARITY(SeqEntry);
    CHECK_ARITY(UnitAnimSeq);
    CHECK_ARITY(Superweapon);
    CHECK_ARITY(RepairSellRules);
    CHECK_ARITY(World::HouseRec);
    CHECK_ARITY(QueueSlot);
    CHECK_ARITY(ProductionState);
    CHECK_ARITY(DifficultyMods);
    CHECK_ARITY(AiRules);
    CHECK_ARITY(PlannedNode);
    CHECK_ARITY(AiBaseState);
    CHECK_ARITY(AiCatalog);
    CHECK_ARITY(TeamInstance);
    CHECK_ARITY(AiTeamState);
    CHECK_ARITY(AiTeamRules);
    CHECK_ARITY(TaskForceMember);
    CHECK_ARITY(TaskForce);
    CHECK_ARITY(ScriptAction);
    CHECK_ARITY(ScriptType);
    CHECK_ARITY(TeamType);
    CHECK_ARITY(TriggerObject);
    CHECK_ARITY(TriggerComparator);
    CHECK_ARITY(AITriggerType);
    CHECK_ARITY(AimdData);
    CHECK_ARITY(AiTriggerState);
    CHECK_ARITY(AiTeamConfig);
    CHECK_ARITY(AiHouseConfig);
    CHECK_ARITY(SkirmishSlot);
    CHECK_ARITY(SkirmishConfig);
    CHECK_ARITY(SceneSave);
    // SaveHeader::version is written OUTSIDE the header block (the magic+version
    // prefix every reader must parse first), so its visitor skips exactly it.
    CHECK(aggregate_arity<SaveHeader>() == visited_fields<SaveHeader>() + 1);
    // P5 veterancy + crates
    CHECK_ARITY(VeterancyRules);
    CHECK_ARITY(WeaponStats);
    CHECK_ARITY(UnitVet);
    CHECK_ARITY(PowerupDef);
    CHECK_ARITY(CrateRules);
    CHECK_ARITY(World::Crate);
    CHECK_ARITY(World::CrateEvent);
    CHECK_ARITY(World::VetEvent);
    // P4 abilities
    CHECK_ARITY(WeaponSpec);
    CHECK_ARITY(AbilityProfile);
    CHECK_ARITY(AbilityRules);
    CHECK_ARITY(RadSite);
    CHECK_ARITY(World::AbilityEvent);
    // Private nested aggregates (through the SaveAccess aliases).
    CHECK_ARITY(SaveAccess::HouseAiRec);
    CHECK_ARITY(SaveAccess::HouseSuperweapon);
    CHECK_ARITY(SaveAccess::TechIncome);
    CHECK_ARITY(SaveAccess::PendingHarvestEvent);
    CHECK_ARITY(SaveAccess::Trig);
    CHECK_ARITY(SaveAccess::Tag);
}

// Non-aggregate classes cannot be counted; their sizeof is pinned on the
// desktop MinGW x64 / libstdc++ ABI (other ABIs skip). A new member changes it
// -- update the visitor in save_fields.hpp, then this number. (A lone bool can
// hide in padding; the round-trip determinism test below covers behavior.)
TEST_CASE("saveload: private-state classes' sizeof is pinned (World et al.)") {
#if defined(__MINGW64__) && defined(__GLIBCXX__) && defined(__x86_64__)
    INFO("a class with private save-state changed size: add the member to "
         "SaveAccess in engine/save_fields.hpp, bump kFormatVersion, re-pin");
    CHECK(sizeof(World) == 3600);
    CHECK(sizeof(MapTriggerSystem) == 360);
    CHECK(sizeof(OreField) == 88);
    CHECK(sizeof(Occupancy) == 32);
    CHECK(sizeof(ProductionQueues) == 224);
    CHECK(sizeof(GameRandom) == 1008);
    CHECK(sizeof(RadField) == 56);
#else
    MESSAGE("sizeof pins are only checked on the MinGW x64 desktop ABI");
#endif
}

// Source-level tripwire for classes whose members cannot be counted: every
// data member named `x_` declared in the class body must be visited by the
// SaveAccess visitor (as `<obj>.x_`). Catches even a lone bool hiding in
// padding. Needs the source tree (RA2_SOURCE_DIR); skips without it.
namespace {
std::string slurp_text(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
std::set<std::string> class_members(const std::string& src, const std::string& head) {
    std::set<std::string> out;
    std::string s;
    for (char ch : src)
        if (ch != '\r') s += ch;   // CRLF checkouts
    const std::size_t b = s.find(head);
    if (b == std::string::npos) return out;
    const std::size_t e = s.find("\n};", b);
    std::string body = s.substr(b, e == std::string::npos ? std::string::npos : e - b);
    body = std::regex_replace(body, std::regex("//[^\\n]*"), "");
    const std::regex mem(R"(\b([a-z_0-9]+_)\s*(=[^;()]*)?;)");
    for (std::sregex_iterator it(body.begin(), body.end(), mem), end; it != end; ++it)
        out.insert((*it)[1].str());
    return out;
}
}  // namespace

TEST_CASE("saveload: every World / MapTriggerSystem data member is visited (source scan)") {
#ifdef RA2_SOURCE_DIR
    const std::string root = RA2_SOURCE_DIR;
    const std::string fields = slurp_text(root + "/engine/save_fields.hpp");
    REQUIRE(!fields.empty());
    struct Case { const char* header; const char* head; const char* obj; std::set<std::string> skip; };
    const Case cases[] = {
        {"/engine/world.hpp", "class World {", "w.", {"map_"}},
        {"/engine/map_trigger.hpp", "class MapTriggerSystem {", "s.", {"d_"}},
    };
    for (const Case& c : cases) {
        const std::set<std::string> m = class_members(slurp_text(root + c.header), c.head);
        CHECK(m.size() > 5);
        for (const std::string& name : m) {
            if (c.skip.count(name)) continue;
            INFO(c.head << " member " << name << " is not serialized: add `save::io(ar, "
                        << c.obj << name << ");` to SaveAccess in engine/save_fields.hpp");
            CHECK(fields.find(std::string(c.obj) + name + ")") != std::string::npos);
        }
    }
#else
    MESSAGE("RA2_SOURCE_DIR not defined; source scan skipped");
#endif
}

// ===========================================================================
// World round trip
// ===========================================================================
namespace {

UnitCombat gun(const char* img, int dmg, double range, bool vehicle = false, bool ballistic = false) {
    UnitCombat c;
    c.strength = 300;
    c.armor = "none";
    c.image = img;
    c.damage = dmg;
    c.rof = 20;
    c.range = range;
    c.sight = 6;
    c.cost = 600;
    c.verses = {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100};
    c.is_vehicle = vehicle;
    if (vehicle) { c.rot = 5; c.turret = true; c.speed_cps = 2.0; }
    if (ballistic) {
        c.proj.kind = ProjKind::Ballistic;
        c.proj.arcing = true;
        c.proj.inaccurate = true;
        c.proj.flak_scatter = true;
        c.proj.weapon_range = static_cast<int>(range * 256);
        c.proj.speed_cap = 102;
        c.proj.flh[2] = 100;
    }
    return c;
}

int free_cell_near(World& w, int x, int y, int& ox, int& oy) {
    for (int r = 0; r <= 12; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                if (w.walkable(x + dx, y + dy) && w.unit_at_cell(x + dx, y + dy) < 0 &&
                    w.building_at_cell(x + dx, y + dy) < 0) {
                    ox = x + dx; oy = y + dy;
                    return 1;
                }
            }
    return 0;
}

// A World with (nearly) every subsystem's firewall open and live state in it.
void populate(World& w) {
    const ra2assets::MapCell* c = pick_start_cell(w.map());
    REQUIRE(c != nullptr);
    const int bx = c->x, by = c->y;
    w.set_house(0, {World::Side::Allied, 0, "Americans", 0, true});
    w.set_house(1, {World::Side::Soviet, 1, "Russians", 1, true});
    w.set_house(2, {World::Side::Neutral, 2, "Neutral", 2, false});
    w.enable_shroud(true);
    w.enable_victory(true, true);
    w.enable_projectiles(ProjectileRules{});
    w.enable_veterancy(VeterancyRules{});
    {
        // P4: open the abilities firewall; the E2s get a death weapon that
        // seeds radiation, the tanks a parasite-able default profile.
        AbilityProfile deso;
        deso.explodes = true;
        deso.death.valid = true;
        deso.death.damage = 50;
        deso.death.cell_spread = 2.0;
        deso.death.rad_level = 200;
        deso.death.verses.assign(11, 100);
        w.set_ability_rules(AbilityRules{});
        w.register_ability_profile("E2", deso);
        w.register_ability_profile("HTNK", AbilityProfile{});
    }
    w.grant_superweapon(1, SuperweaponType::Nuke, 30.0, 600, 3.0, 0.2,
                        {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100});
    std::vector<BuildingTypeDef> types(2);
    types[0].id = "GAPOWR"; types[0].cost = 800; types[0].strength = 750; types[0].fw = 2;
    types[0].fh = 2; types[0].power = 100; types[0].crewed = true;
    types[1].id = "NAPOWR"; types[1].cost = 600; types[1].strength = 750; types[1].fw = 2;
    types[1].fh = 2; types[1].power = 100;
    w.set_building_types(types);
    UnitCombat crew = gun("E1", 10, 3.0);
    w.set_survivor_crew(0, crew);

    int ox = 0, oy = 0;
    // Two buildings, one being repaired after taking damage.
    int b0 = -1, b1 = -1;
    if (free_cell_near(w, bx - 6, by - 6, ox, oy))
        b0 = w.add_building(0, ox, oy, 1, 1, 0, 750, 0, 0.0, true, 5);
    if (free_cell_near(w, bx + 7, by + 7, ox, oy))
        b1 = w.add_building(1, ox, oy, 1, 1, 1, 750, 0, 2.0, false, 5);
    if (b0 >= 0) {
        w.buildings()[b0].hp = 400;
        w.grant_credits_for_test(0, 5000.0);
        w.set_repair(b0, 0, 1);
    }
    (void)b1;
    // Opposing armies: tanks with arcing scattered shells + infantry.
    for (int k = 0; k < 4; ++k) {
        if (free_cell_near(w, bx - 2, by + k, ox, oy))
            w.add_combat_unit(ox, oy, 0, gun("HTNK", 40, 6.0, true, true));
        if (free_cell_near(w, bx + 3, by + k, ox, oy))
            w.add_combat_unit(ox, oy, 1, gun("E2", 15, 4.0));
    }
    // A selected group order so paths/formations are in flight.
    for (Unit& u : w.units())
        if (u.house == 0) u.selected = true;
    w.order_move_selected(bx + 2, by + 2);
    w.clear_selection();
    // Disguise + a pending spy order to keep capture state non-default.
    if (!w.units().empty()) w.set_disguise(static_cast<int>(w.units().size()) - 1, "E1", 0);
}

}  // namespace

TEST_CASE("saveload: rich World round-trips and continues bit-identically") {
    if (!ra2fixture::mapsmd03_available()) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    const ra2assets::MapFile& map = ra2fixture::mapsmd03_map();
    World a(map);
    populate(a);
    for (int f = 0; f < 45; ++f) a.step(1.0 / 15.0);
    const bool shells = !a.projectiles().empty();
    MESSAGE("projectiles in flight at save: " << a.projectiles().size());

    const std::vector<std::uint8_t> s0 = save_world(a);
    World b(map);
    REQUIRE(load_world(b, s0));
    CHECK(save_world(b) == s0);               // save(load(x)) == x
    CHECK(save_world(a) == s0);               // saving is read-only

    for (int f = 0; f < 90; ++f) {
        a.step(1.0 / 15.0);
        b.step(1.0 / 15.0);
    }
    const std::vector<std::uint8_t> sa = save_world(a), sb = save_world(b);
    CHECK(sa == sb);                          // continued bit-identically
    CHECK(sa != s0);                          // ...and the sim actually moved
    (void)shells;
}

TEST_CASE("saveload: load_world rejects a truncated blob") {
    ra2assets::MapFile map;
    World a(map);
    a.add_combat_unit(0, 0, 0, gun("GI", 10, 3.0));
    std::vector<std::uint8_t> s = save_world(a);
    s.resize(s.size() / 2);
    World b(map);
    CHECK_FALSE(load_world(b, s));
}

// ===========================================================================
// Map-trigger runtime round trip
// ===========================================================================
namespace {
struct NullHost : TriggerHost {
    bool house_exists(int ht) const override { return ht == 0; }
    std::vector<int> house_instances() const override { return {0}; }
    int player_house_type() const override { return 0; }
    int house_buildings(int) const override { return 1; }
    int house_vehicles(int) const override { return 0; }
    int house_infantry(int) const override { return 0; }
    int house_building_type_count(int, int) const override { return 0; }
    int techno_count(const std::string&) const override { return 0; }
    bool apply(const TriggerEffect&) override { return true; }
};
const char* kTrigMap =
    "[Countries]\n0=Player\n"
    "[Triggers]\n"
    "TRGA=Player,<none>,A,0,1,1,1,0\n"
    "TRGB=Player,<none>,B,0,1,1,1,0\n"
    "TRGC=Player,<none>,C,0,1,1,1,0\n"
    "[Events]\nTRGA=1,13,0,2\nTRGB=1,51,0,3\nTRGC=1,36,0,1\n"
    "[Actions]\nTRGA=1,56,0,1,0,0,0,0,A\nTRGB=1,57,0,1,0,0,0,0,A\nTRGC=1,56,0,2,0,0,0,0,A\n"
    "[Tags]\nTAGA=2,a,TRGA\nTAGB=2,b,TRGB\nTAGC=2,c,TRGC\n";
}  // namespace

TEST_CASE("saveload: MapTriggerSystem round-trips timers/latches/RNG/log") {
    const MapTriggerData d = parse_map_triggers(ra2assets::Ini::parse(kTrigMap), {"Player"}, {});
    NullHost h;
    MapTriggerSystem a(d, 1, 0x1234u);
    a.start(h);
    for (int i = 0; i < 77; ++i) a.tick(h);
    const std::vector<std::uint8_t> s0 = save_triggers(a);

    MapTriggerSystem b(d, 1, 0x9999u);   // a different seed: load must overwrite it
    REQUIRE(load_triggers(b, s0));
    CHECK(save_triggers(b) == s0);
    for (int i = 0; i < 300; ++i) {
        a.tick(h);
        b.tick(h);
    }
    CHECK(a.state_hash() == b.state_hash());
    CHECK(a.log() == b.log());
    CHECK(save_triggers(a) == save_triggers(b));
    CHECK(a.fired_count() > 3);
}

// ===========================================================================
// File container + slots
// ===========================================================================
TEST_CASE("saveload: container encodes header + sections; wrong version is flagged") {
    SaveFile f;
    f.header.description = std::string(100, 'x') + "\n";   // clamped to 80
    f.header.map_file = "xmp31s2.map";
    f.header.frame = 4242;
    f.sections[kTagWorld] = {1, 2, 3};
    f.sections[kTagScene] = {};
    std::vector<std::uint8_t> bytes = encode_save(f);
    SaveFile g;
    REQUIRE(decode_save(bytes, g));
    CHECK(g.header.version == kFormatVersion);
    CHECK(g.header.description.size() == kMaxDescription);
    CHECK(g.header.map_file == "xmp31s2.map");
    CHECK(g.header.frame == 4242);
    REQUIRE(g.section(kTagWorld));
    CHECK(*g.section(kTagWorld) == std::vector<std::uint8_t>{1, 2, 3});
    CHECK(g.section(kTagTriggers) == nullptr);

    std::vector<std::uint8_t> other = bytes;
    other[8] ^= 0x7F;   // version field
    SaveFile h;
    REQUIRE(decode_save(other, h));
    CHECK(h.header.version != kFormatVersion);
    CHECK(h.sections.empty());

    std::vector<std::uint8_t> junk = {'M', 'Z', 0, 0};
    CHECK_FALSE(decode_save(junk, h));
}

TEST_CASE("saveload: slots - SAVE%04X.SAV naming, newest-first listing, delete") {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "ra2_p7_saveload_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    const std::string n1 = new_save_name(dir.string(), 7);
    CHECK(n1.size() == 12);
    CHECK(n1.rfind("SAVE", 0) == 0);
    CHECK(n1.substr(8) == ".SAV");
    SaveFile f;
    f.header.description = "first";
    REQUIRE(write_file((dir / n1).string(), encode_save(f)));
    const std::string n2 = new_save_name(dir.string(), 7);
    CHECK(n2 != n1);   // retried past the taken name
    f.header.description = "second";
    REQUIRE(write_file((dir / n2).string(), encode_save(f)));
    fs::last_write_time(dir / n1, fs::last_write_time(dir / n2) - std::chrono::hours(1));
    { std::FILE* fp = std::fopen((dir / "notes.txt").string().c_str(), "wb"); if (fp) std::fclose(fp); }
    { std::FILE* fp = std::fopen((dir / "JUNK.SAV").string().c_str(), "wb");
      if (fp) { std::fputs("nope", fp); std::fclose(fp); } }

    std::vector<SlotEntry> s = list_saves(dir.string());
    REQUIRE(s.size() == 2);
    CHECK(s[0].header.description == "second");   // newest first
    CHECK(s[1].header.description == "first");
    CHECK_FALSE(s[0].wrong_version);
    CHECK(delete_save(s[1].path));
    CHECK(list_saves(dir.string()).size() == 1);
    fs::remove_all(dir, ec);
    CHECK(clean_description("a\tb\x01" "c") == "abc");
}

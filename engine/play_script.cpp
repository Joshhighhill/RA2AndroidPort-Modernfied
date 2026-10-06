#include "play_script.hpp"

#include <cstdlib>
#include <set>
#include <sstream>

namespace ra2engine {
namespace play {

namespace {
const std::set<std::string>& known_ops() {
    static const std::set<std::string> k = {"wait", "click", "select", "drag", "move", "key",
                                            "shot", "assert", "until", "cheat", "expect", "end"};
    return k;
}
bool to_int(const std::string& s, long long& v) {
    if (s.empty()) return false;
    char* e = nullptr;
    v = std::strtoll(s.c_str(), &e, 10);
    return e && *e == '\0';
}
}  // namespace

bool parse_script(const std::string& text, std::vector<Command>& out, std::string& err) {
    std::istringstream in(text);
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        ++n;
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream ls(line);
        Command c;
        c.line = n;
        if (!(ls >> c.op)) continue;
        std::string a;
        while (ls >> a) c.args.push_back(a);
        if (!known_ops().count(c.op)) {
            err = "line " + std::to_string(n) + ": unknown command '" + c.op + "'";
            return false;
        }
        long long v = 0;
        if ((c.op == "wait") && (c.args.size() != 1 || !to_int(c.args[0], v) || v < 0)) {
            err = "line " + std::to_string(n) + ": wait needs one non-negative integer";
            return false;
        }
        if (c.op == "assert" && c.args.size() != 3) {
            err = "line " + std::to_string(n) + ": assert needs '<key> <op> <int>'";
            return false;
        }
        if (c.op == "until" && (c.args.size() != 4 || !to_int(c.args[0], v))) {
            err = "line " + std::to_string(n) + ": until needs 'N <key> <op> <int>'";
            return false;
        }
        if ((c.op == "click" || c.op == "drag" || c.op == "move" || c.op == "key" ||
             c.op == "shot" || c.op == "select" || c.op == "cheat" || c.op == "expect") &&
            c.args.empty()) {
            err = "line " + std::to_string(n) + ": " + c.op + " needs arguments";
            return false;
        }
        out.push_back(c);
    }
    return true;
}

bool eval_assert(const std::vector<std::string>& expr, const Probe& p, bool& result,
                 std::string& err) {
    if (expr.size() != 3) {
        err = "expression needs three tokens";
        return false;
    }
    long long rhs = 0;
    if (!to_int(expr[2], rhs)) {
        err = "right-hand side '" + expr[2] + "' is not an integer";
        return false;
    }
    const auto it = p.find(expr[0]);
    const long long lhs = (it == p.end()) ? 0 : it->second;
    const std::string& op = expr[1];
    if (op == "==") result = lhs == rhs;
    else if (op == "!=") result = lhs != rhs;
    else if (op == "<") result = lhs < rhs;
    else if (op == "<=") result = lhs <= rhs;
    else if (op == ">") result = lhs > rhs;
    else if (op == ">=") result = lhs >= rhs;
    else {
        err = "unknown operator '" + op + "'";
        return false;
    }
    return true;
}

std::string probe_line(const Probe& p) {
    std::string s;
    for (const auto& [k, v] : p) {
        if (!s.empty()) s += ' ';
        s += k + "=" + std::to_string(v);
    }
    return s;
}

uint64_t fnv1a(const std::string& s, uint64_t h) {
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

bool parse_unit_selector(const std::string& s, int& house, std::string& image, int& n) {
    const std::size_t a = s.find(':');
    if (a == std::string::npos || a == 0) return false;
    long long h = 0;
    if (!to_int(s.substr(0, a), h)) return false;
    const std::size_t b = s.find(':', a + 1);
    image = s.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
    if (image.empty()) return false;
    long long k = 0;
    if (b != std::string::npos && !to_int(s.substr(b + 1), k)) return false;
    house = static_cast<int>(h);
    n = static_cast<int>(k);
    return true;
}

}  // namespace play
}  // namespace ra2engine

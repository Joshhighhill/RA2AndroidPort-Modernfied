#include "play_script.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace ra2play {
namespace {

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Strip a '#' comment that is not inside double quotes.
std::string strip_comment(const std::string& s) {
    bool q = false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '"') q = !q;
        else if (s[i] == '#' && !q) return s.substr(0, i);
    }
    return s;
}

// Whitespace split; a "quoted run" is one token (quotes stripped).
std::vector<std::string> split_words(const std::string& s) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i >= s.size()) break;
        if (s[i] == '"') {
            const std::size_t e = s.find('"', i + 1);
            out.push_back(s.substr(i + 1, (e == std::string::npos ? s.size() : e) - i - 1));
            i = (e == std::string::npos) ? s.size() : e + 1;
        } else {
            std::size_t e = i;
            while (e < s.size() && !std::isspace(static_cast<unsigned char>(s[e]))) ++e;
            out.push_back(s.substr(i, e - i));
            i = e;
        }
    }
    return out;
}

bool to_int(const std::string& s, int& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    const long x = std::strtol(s.c_str(), &end, 10);
    if (*end) return false;
    v = static_cast<int>(x);
    return true;
}

bool to_num(const std::string& s, double& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    v = std::strtod(s.c_str(), &end);
    return *end == 0;
}

const std::set<std::string>& known_ops() {
    static const std::set<std::string> k = {
        "set",  "wait", "click", "drag",   "move",    "key",   "hold", "type", "view",
        "place", "shot", "assert", "until", "require", "let",  "cheat", "log", "end",
        "expect", "dump"};
    return k;
}

}  // namespace

bool parse_script(const std::string& text, Script& out, std::string& err) {
    out.cmds.clear();
    std::istringstream in(text);
    std::string line;
    int ln = 0;
    while (std::getline(in, line)) {
        ++ln;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = trim(strip_comment(line));
        if (t.empty() || t[0] == ';') continue;
        Cmd c;
        c.line = ln;
        c.raw = t;
        // Split off trailing @tags (they never appear inside expressions).
        std::vector<std::string> words = split_words(t);
        while (!words.empty() && words.back().size() > 1 && words.back()[0] == '@') {
            c.tags.insert(c.tags.begin(), words.back().substr(1));
            words.pop_back();
        }
        if (words.empty()) continue;
        c.op = lower(words[0]);
        if (!known_ops().count(c.op)) {
            err = "line " + std::to_string(ln) + ": unknown command '" + words[0] + "'";
            return false;
        }
        c.args.assign(words.begin() + 1, words.end());
        // The expression text = everything after the op (and N for until),
        // minus the tags, re-joined from the raw words (quotes kept).
        auto rejoin = [&](std::size_t from) {
            std::string e;
            // Re-tokenise the raw line keeping quotes so string literals survive.
            std::vector<std::string> raw;
            {
                std::size_t i = 0;
                const std::string& s = t;
                while (i < s.size()) {
                    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
                    if (i >= s.size()) break;
                    std::size_t e2 = i;
                    if (s[i] == '"') {
                        e2 = s.find('"', i + 1);
                        e2 = (e2 == std::string::npos) ? s.size() : e2 + 1;
                    } else {
                        while (e2 < s.size() && !std::isspace(static_cast<unsigned char>(s[e2]))) ++e2;
                    }
                    raw.push_back(s.substr(i, e2 - i));
                    i = e2;
                }
            }
            const std::size_t stop = raw.size() - c.tags.size();
            for (std::size_t k = from; k < stop; ++k) e += (e.empty() ? "" : " ") + raw[k];
            return e;
        };
        auto need = [&](std::size_t n) {
            if (c.args.size() < n) {
                err = "line " + std::to_string(ln) + ": '" + c.op + "' needs " + std::to_string(n) +
                      " argument(s)";
                return false;
            }
            return true;
        };
        if (c.op == "wait") {
            if (!need(1) || !to_int(c.args[0], c.n) || c.n < 0) {
                if (err.empty()) err = "line " + std::to_string(ln) + ": wait N";
                return false;
            }
        } else if (c.op == "until") {
            if (!need(2) || !to_int(c.args[0], c.n) || c.n < 0) {
                if (err.empty()) err = "line " + std::to_string(ln) + ": until N EXPR";
                return false;
            }
            c.expr = rejoin(2);
        } else if (c.op == "assert" || c.op == "require") {
            if (!need(1)) return false;
            c.expr = rejoin(1);
        } else if (c.op == "let") {
            if (c.args.size() < 3 || c.args[1] != "=") {
                err = "line " + std::to_string(ln) + ": let NAME = OPERAND";
                return false;
            }
            c.expr = rejoin(3);
        } else if (c.op == "hold") {
            if (!need(2) || !to_int(c.args[1], c.n) || c.n < 1) {
                if (err.empty()) err = "line " + std::to_string(ln) + ": hold KEY N";
                return false;
            }
        } else if (c.op == "click" || c.op == "key" || c.op == "shot" || c.op == "set" ||
                   c.op == "type" || c.op == "cheat" || c.op == "move" || c.op == "view" ||
                   c.op == "drag" || c.op == "place" || c.op == "expect") {
            if (!need(1)) return false;
        }
        out.cmds.push_back(std::move(c));
    }
    return true;
}

// ---------------------------------------------------------------------------
bool resolve_operand(const std::string& tok, const ProbeData& p,
                     const std::map<std::string, Value>& vars, bool lhs, Value& out) {
    out = Value{};
    if (tok.empty()) return false;
    double d = 0.0;
    if (to_num(tok, d)) { out.is_num = true; out.num = d; return true; }
    if (tok.size() >= 2 && tok.front() == '"' && tok.back() == '"') {
        out.is_num = false;
        out.str = tok.substr(1, tok.size() - 2);
        return true;
    }
    if (tok[0] == '$') {
        auto it = vars.find(tok.substr(1));
        if (it == vars.end()) return false;
        out = it->second;
        return true;
    }
    if (auto it = p.n.find(tok); it != p.n.end()) { out.is_num = true; out.num = it->second; return true; }
    if (auto it = p.s.find(tok); it != p.s.end()) { out.is_num = false; out.str = it->second; return true; }
    const std::size_t par = tok.find('(');
    if (par != std::string::npos && p.zero_families.count(tok.substr(0, par))) {
        out.is_num = true;
        out.num = 0.0;
        return true;
    }
    if (!lhs) { out.is_num = false; out.str = tok; return true; }   // bare word literal
    return false;
}

namespace {

std::string show(const Value& v) {
    if (!v.is_num) return "\"" + v.str + "\"";
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v.num);
    return b;
}

Eval eval_clause(const std::vector<std::string>& w, const ProbeData& p,
                 const std::map<std::string, Value>& vars, std::string& detail) {
    static const std::set<std::string> ops = {"==", "!=", "<", "<=", ">", ">="};
    if (w.size() == 1) {
        Value v;
        if (!resolve_operand(w[0], p, vars, true, v)) { detail = "missing-probe:" + w[0]; return Eval::Missing; }
        detail = w[0] + "=" + show(v);
        const bool t = v.is_num ? v.num != 0.0 : !v.str.empty();
        return t ? Eval::True : Eval::False;
    }
    if (w.size() != 3 || !ops.count(w[1])) {
        detail = "malformed clause";
        return Eval::Error;
    }
    Value a, b;
    if (!resolve_operand(w[0], p, vars, true, a)) { detail = "missing-probe:" + w[0]; return Eval::Missing; }
    if (!resolve_operand(w[2], p, vars, false, b)) { detail = "missing-probe:" + w[2]; return Eval::Missing; }
    detail = w[0] + "=" + show(a) + " " + w[1] + " " + show(b);
    const std::string& op = w[1];
    bool r = false;
    if (a.is_num && b.is_num) {
        const double x = a.num, y = b.num;
        r = op == "==" ? std::fabs(x - y) < 1e-9 : op == "!=" ? std::fabs(x - y) >= 1e-9
          : op == "<" ? x < y : op == "<=" ? x <= y : op == ">" ? x > y : x >= y;
    } else {
        const std::string x = a.is_num ? show(a) : a.str, y = b.is_num ? show(b) : b.str;
        if (op == "==") r = x == y;
        else if (op == "!=") r = x != y;
        else { detail += " (ordering on a string)"; return Eval::Error; }
    }
    return r ? Eval::True : Eval::False;
}

}  // namespace

Eval eval_expr(const std::string& expr, const ProbeData& p,
               const std::map<std::string, Value>& vars, std::string& detail) {
    // Tokenise keeping "quoted strings" as single tokens WITH quotes.
    std::vector<std::string> toks;
    {
        std::size_t i = 0;
        while (i < expr.size()) {
            while (i < expr.size() && std::isspace(static_cast<unsigned char>(expr[i]))) ++i;
            if (i >= expr.size()) break;
            std::size_t e = i;
            if (expr[i] == '"') {
                e = expr.find('"', i + 1);
                e = (e == std::string::npos) ? expr.size() : e + 1;
            } else {
                while (e < expr.size() && !std::isspace(static_cast<unsigned char>(expr[e]))) ++e;
            }
            toks.push_back(expr.substr(i, e - i));
            i = e;
        }
    }
    if (toks.empty()) { detail = "empty expression"; return Eval::Error; }
    detail.clear();
    Eval acc = Eval::True;
    std::string join = "and";
    std::vector<std::string> cur;
    auto flush = [&]() -> bool {
        std::string d;
        const Eval e = eval_clause(cur, p, vars, d);
        detail += (detail.empty() ? "" : " " + join + " ") + d;
        cur.clear();
        if (e == Eval::Missing || e == Eval::Error) { acc = e; return false; }
        const bool a = acc == Eval::True, b = e == Eval::True;
        acc = (join == "and" ? (a && b) : (a || b)) ? Eval::True : Eval::False;
        return true;
    };
    bool first = true;
    for (const std::string& t : toks) {
        if (t == "and" || t == "or") {
            if (cur.empty()) { detail = "dangling '" + t + "'"; return Eval::Error; }
            if (first) {
                std::string d;
                const Eval e = eval_clause(cur, p, vars, d);
                detail = d;
                cur.clear();
                if (e == Eval::Missing || e == Eval::Error) return e;
                acc = e;
                first = false;
            } else if (!flush()) {
                return acc;
            }
            join = t;
            continue;
        }
        cur.push_back(t);
    }
    if (cur.empty()) { detail = "dangling operator"; return Eval::Error; }
    if (first) {
        std::string d;
        const Eval e = eval_clause(cur, p, vars, d);
        detail = d;
        return e;
    }
    flush();
    return acc;
}

std::string probe_dump(const ProbeData& p) {
    std::string out;
    char b[64];
    for (const auto& [k, v] : p.n) {
        std::snprintf(b, sizeof b, "%.6g", v);
        out += k + "=" + b + "\n";
    }
    for (const auto& [k, v] : p.s) out += k + "=\"" + v + "\"\n";
    return out;
}

std::uint64_t probe_hash(const ProbeData& p, std::uint64_t h) {
    const std::string d = probe_dump(p);
    for (unsigned char c : d) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

}  // namespace ra2play

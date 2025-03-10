#include "co/def.h"
#include "co/str.h"
#include "co/fs.h"
#include "co/flag.h"
#include "co/log.h"

void generate(const fastring& gen_file, const fastring& pkg, const fastring& serv, 
              const std::vector<fastring>& methods) {
    fs::fstream fs(gen_file.c_str(), 'w');

    do {
        fs << "// Autogenerated, do not edit. All changes will be undone.\n\n";
        fs << "#pragma once\n\n";
        fs << "#include \"co/so/rpc.h\"\n";
        fs << "#include \"co/hash.h\"\n";
        fs << "#include <unordered_map>\n\n";
    } while (0);

    auto pkgs = str::split(pkg, '.');
    for (size_t i = 0; i < pkgs.size(); ++i) {
        fs << "namespace " << pkgs[i] << " {\n";
    }
    if (!pkgs.empty()) fs << "\n";

    fs << "class " << serv << " : public rpc::Service {\n";
    fs << "  public:\n";
    fs << fastring(' ', 4) << "typedef void (" << serv << "::*Fun)(const Json&, Json&);\n\n";

    do {
        fs << fastring(' ', 4) << serv << "() {\n";
        fs << fastring(' ', 8) << "_methods[hash64(\"ping\")] = &" << serv << "::ping;\n";
        for (size_t i = 0; i < methods.size(); ++i) {
            fs << fastring(' ', 8) << "_methods[hash64(\"" << methods[i] << "\")] = &"
               << serv << "::" << methods[i] << ";\n";
        }
        fs << fastring(' ', 4) << "}\n\n";

        fs << fastring(' ', 4) << "virtual ~" << serv << "() {}\n\n";
    } while (0);

    // virtual void process(const Json& req, Json& res)
    do {
        fs << fastring(' ', 4) << "virtual void process(const Json& req, Json& res) {\n";
        fs << fastring(' ', 8) << "json::Value method = req[\"method\"];\n";
        fs << fastring(' ', 8) << "if (!method.is_string()) {\n";
        fs << fastring(' ', 12) << "res.add_member(\"err\", 400);\n";
        fs << fastring(' ', 12) << "res.add_member(\"errmsg\", \"400 req has no method\");\n";
        fs << fastring(' ', 12) << "return;\n";
        fs << fastring(' ', 8) << "}\n\n";

        fs << fastring(' ', 8) << "auto it = _methods.find(hash64(method.get_string(), method.size()));\n";
        fs << fastring(' ', 8) << "if (it == _methods.end()) {\n";
        fs << fastring(' ', 12) << "res.add_member(\"err\", 404);\n";
        fs << fastring(' ', 12) << "res.add_member(\"errmsg\", \"404 method not found\");\n";
        fs << fastring(' ', 12) << "return;\n";
        fs << fastring(' ', 8) << "}\n\n";

        fs << fastring(' ', 8) << "(this->*it->second)(req, res);\n";
        fs << fastring(' ', 4) << "}\n\n";
    } while (0);

    // virtual void ping(const Json& req, Json& res)
    do {
        fs << fastring(' ', 4) << "virtual void ping(const Json& req, Json& res) {\n";
        fs << fastring(' ', 8) << "res.add_member(\"method\", \"ping\");\n";
        fs << fastring(' ', 8) << "res.add_member(\"err\", 200);\n";
        fs << fastring(' ', 8) << "res.add_member(\"errmsg\", \"pong\");\n";
        fs << fastring(' ', 4) << "}\n\n";
    } while (0);

    // virtual void xxx(const Json& req, Json& res)
    for (size_t i = 0; i < methods.size(); ++i) {
        fs << fastring(' ', 4) << "virtual void " << methods[i] << "(const Json& req, Json& res) = 0;\n\n";
    }

    fs << "  private:\n";
    fs << "    std::unordered_map<uint64, Fun> _methods;\n";
    fs << "};\n";
    if (!pkgs.empty()) fs << '\n';

    for (size_t i = 0; i < pkgs.size(); ++i) {
        fs << "} // " << pkgs[i] << "\n";
    }

    fs.flush();
}

void parse(const char* path) {
    fs::file f;
    if (!f.open(path, 'r')) {
        COUT << "failed to open file: " << path;
        exit(-1);
    }

    const char* b = strrchr(path, '/');
    if (b == 0) b = strrchr(path, '\\');
    b == 0 ? (b = path) : ++b;
    const char* e = strrchr(path, '.');

    if (e == 0 || e <= b) {
        COUT << "invalid proto file name: " << path;
        exit(-1);
    }

    fastring gen_file(b, e - b);
    gen_file += ".h";
    fastring pkg;
    fastring serv;
    std::vector<fastring> methods;

    auto s = f.read(fs::fsize(path));
    char c = '\n';
    if (!strchr(s.c_str(), '\n') && strchr(s.c_str(), '\r')) c = '\r';

    auto l = str::split(s.c_str(), c);

    for (size_t i = 0; i < l.size(); ++i) {
        auto x = str::strip(l[i]);
        if (x.empty()) continue;
        if (x.starts_with("//")) continue;

        if (x.starts_with("package ")) {
            if (!pkg.empty()) {
                COUT << "find multiple package name in file: " << path;
                exit(-1);
            }

            const char* p = strstr(x.c_str(), "//");
            if (p) x.resize(p - x.data());
            pkg = x.c_str() + 8; 
            pkg = str::strip(pkg);
            continue;
        }

        if (x.starts_with("service ")) {
            if (!serv.empty()) {
                COUT << "find multiple service in file: " << path;
                exit(-1);
            }

            const char* p = strstr(x.c_str(), "//");
            if (p) x.resize(p - x.data());
            serv = x.c_str() + 8;
            serv = str::strip(serv, " \t\r\n{");

            for (size_t k = i + 1; k < l.size(); ++k) {
                const char* p = strstr(l[k].c_str(), "//");
                if (p) l[k].resize(p - l[k].data());
 
                if (l[k].find('}') != l[k].npos) {
                    auto m = str::strip(l[k], " \t\r\n,;{}");
                    if (!m.empty()) methods.push_back(m);
                    if (methods.empty()) {
                        COUT << "no method found in service: " << serv;
                        exit(-1);
                    }

                    generate(gen_file, pkg, serv, methods);
                    COUT << "generate " << gen_file << " success";
                    return;
                } else {
                    auto m = str::strip(l[k], " \t\r\n,;{");
                    if (!m.empty()) methods.push_back(m);
                }
            }

            COUT << "ending '}' not found for service: " << serv;
            exit(-1);
        }
    }
}

int main(int argc, char** argv) {
    auto v = flag::init(argc, argv);
    log::init();
    if (v.empty()) {
        COUT << "usage: gen xx.proto";
        return 0;
    }

    for (size_t i = 0; i < v.size(); ++i) {
        parse(v[i].c_str());
    }

    return 0;
}

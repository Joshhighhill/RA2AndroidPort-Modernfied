#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "ra2assets/mix.hpp"
#include "ra2assets/pal.hpp"
#include "ra2assets/shp.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace ra2assets;

static std::vector<uint8_t> slurp(const std::string& p){
    std::ifstream f(p,std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),{});
}

// Descend a nested-mix chain: "cache.mix" or "a.mix/b.mix".
static MixFile descend(MixFile m, std::string path){
    while (!path.empty()){
        size_t s = path.find('/');
        std::string head = (s==std::string::npos) ? path : path.substr(0,s);
        path = (s==std::string::npos) ? "" : path.substr(s+1);
        m = m.open_nested(head);
    }
    return m;
}

// Resolve "name.shp" or "inner.mix/name.shp" (any depth) to entry bytes.
static std::vector<uint8_t> read_path(const MixFile& m, const std::string& path){
    size_t s = path.find('/');
    if (s == std::string::npos){
        const MixEntry* e = m.find(path);
        if (!e) throw std::runtime_error("not found: " + path);
        return m.read(*e);
    }
    MixFile inner = m.open_nested(path.substr(0,s));
    return read_path(inner, path.substr(s+1));
}

int main(int argc, char** argv){
    if (argc<3){
        std::cerr<<"usage: ra2extract <mix> --list [nested-path]\n"
                 <<"       ra2extract <mix> --raw <asset-path> <out.bin>\n"
                 <<"       ra2extract <mix> <shp-path> <pal-path> <out.png>\n"
                 <<"       asset paths may descend nested mixes: conquer.mix/e1.shp\n";
        return 2;
    }
    try {
        auto bytes=slurp(argv[1]);
        if (bytes.empty()){ std::cerr<<"cannot read "<<argv[1]<<"\n"; return 1; }
        MixFile m = MixFile::open(std::move(bytes));
        if (std::string(argv[2])=="--list"){
            MixFile t = (argc>=4) ? descend(std::move(m), argv[3]) : std::move(m);
            std::cout<<"entries: "<<t.count()<<"\n";
            for (auto& e : t.entries())
                std::printf("  %08X  offset=%-10u size=%u\n", e.id, e.offset, e.size);
            for (auto& n : t.local_names()) std::cout<<"  "<<n<<"\n";
            return 0;
        }
        if (std::string(argv[2])=="--raw"){
            // Dump the raw extracted bytes of any nested asset to disk, so an
            // external oracle (e.g. XCC's own decoder) can be fed byte-identical
            // input. Extraction is ra2assets'; decoding is the oracle's.
            if (argc<5){ std::cerr<<"need --raw <asset-path> <out.bin>\n"; return 2; }
            auto raw=read_path(m, argv[3]);
            std::ofstream out(argv[4], std::ios::binary);
            if (!out){ std::cerr<<"cannot write "<<argv[4]<<"\n"; return 1; }
            out.write(reinterpret_cast<const char*>(raw.data()), (std::streamsize)raw.size());
            std::cout<<"wrote "<<argv[4]<<" ("<<raw.size()<<" bytes)\n";
            return 0;
        }
        if (argc<5){ std::cerr<<"need <shp-path> <pal-path> <out.png>\n"; return 2; }
        ShpFile s=ShpFile::load(read_path(m, argv[2]));
        auto praw=read_path(m, argv[3]);
        Palette pal=Palette::load(praw.data(),praw.size());
        auto idx=s.decode_indexed(0);
        auto rgba=ShpFile::palettize(idx,pal);
        auto fi=s.frame_info(0);
        stbi_write_png(argv[4], fi.w, fi.h, 4, rgba.data(), fi.w*4);
        std::cout<<"wrote "<<argv[4]<<" ("<<fi.w<<"x"<<fi.h<<", "<<s.count()<<" frames)\n";
        return 0;
    } catch (const std::exception& ex){
        std::cerr<<"error: "<<ex.what()<<"\n";
        return 1;
    }
}

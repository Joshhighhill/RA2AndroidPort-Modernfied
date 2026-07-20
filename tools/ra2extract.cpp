#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "ra2assets/fileio.hpp"
#include "ra2assets/mix.hpp"
#include "ra2assets/pal.hpp"
#include "ra2assets/shp.hpp"
#include "ra2assets/pcx.hpp"
#include <cstdio>
#include <string>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace ra2assets;

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
                 <<"       ra2extract <mix> <shp-path> <pal-path> <out.png> [frame]\n"
                 <<"       asset paths may descend nested mixes: conquer.mix/e1.shp\n"
                 <<"       [frame] defaults to 0; must be < the SHP's frame count.\n";
        return 2;
    }
    try {
        auto bytes=read_file(argv[1]);
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
        if (std::string(argv[2])=="--dumpall"){
            // Bulk-dump every entry of a nested mix as PNG, auto-detecting PCX
            // (byte0==0x0A) vs SHP (try frame 0). Names outputs by hash id so an
            // asset can be located visually then reverse-matched. Palette used for
            // SHP frames only (PCX is self-palettized).
            // usage: <mix> --dumpall <nested-mix-path> <outdir> <pal-path>
            if (argc<6){ std::cerr<<"need --dumpall <nested> <outdir> <pal>\n"; return 2; }
            MixFile t = descend(std::move(m), argv[3]);
            std::string outdir = argv[4];
            std::vector<uint8_t> praw;
            try { praw = read_file(argv[5]); } catch(...){}
            // pal path may be nested in the ORIGINAL mix; reload for pal
            Palette pal;
            bool havepal=false;
            try {
                auto pbytes=read_file(argv[1]);
                MixFile pm=MixFile::open(std::move(pbytes));
                auto pr=read_path(pm, argv[5]);
                pal=Palette::load(pr.data(),pr.size()); havepal=true;
            } catch(...){}
            int npcx=0,nshp=0,nfail=0;
            for (auto& e : t.entries()){
                std::vector<uint8_t> b;
                try { b=t.read(e); } catch(...){ continue; }
                if (b.empty()) continue;
                char nm[1024];
                if (b[0]==0x0A){ // PCX
                    try {
                        PcxImage img=decode_pcx(b);
                        auto rgba=img.rgba();
                        std::snprintf(nm,sizeof nm,"%s/pcx_%08X_%dx%d.png",outdir.c_str(),e.id,img.w,img.h);
                        stbi_write_png(nm,img.w,img.h,4,rgba.data(),img.w*4);
                        npcx++; continue;
                    } catch(...){}
                }
                if (havepal){ // try SHP frame 0
                    try {
                        ShpFile s=ShpFile::load(b);
                        if (s.count()>0){
                            auto fi=s.frame_info(0);
                            if (fi.w>0&&fi.h>0&&fi.w<4096&&fi.h<4096){
                                auto idx=s.decode_indexed(0);
                                auto rgba=ShpFile::palettize(idx,pal);
                                std::snprintf(nm,sizeof nm,"%s/shp_%08X_%dx%d_f%d.png",outdir.c_str(),e.id,fi.w,fi.h,s.count());
                                stbi_write_png(nm,fi.w,fi.h,4,rgba.data(),fi.w*4);
                                nshp++; continue;
                            }
                        }
                    } catch(...){}
                }
                nfail++;
            }
            std::cout<<"dumpall: pcx="<<npcx<<" shp="<<nshp<<" other="<<nfail<<"\n";
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
        // PCX -> PNG: <mix> <pcx-path> <out.png>  (self-palettized 8-bit RLE PCX)
        {
            std::string p2(argv[2]);
            if (p2.size()>=4 && p2.compare(p2.size()-4,4,".pcx")==0){
                if (argc<4){ std::cerr<<"need <pcx-path> <out.png>\n"; return 2; }
                PcxImage img = decode_pcx(read_path(m, argv[2]));
                auto rgba = img.rgba();
                stbi_write_png(argv[3], img.w, img.h, 4, rgba.data(), img.w*4);
                std::cout<<"wrote "<<argv[3]<<" ("<<img.w<<"x"<<img.h<<", pcx)\n";
                return 0;
            }
        }
        if (argc<5){ std::cerr<<"need <shp-path> <pal-path> <out.png>\n"; return 2; }
        ShpFile s=ShpFile::load(read_path(m, argv[2]));
        if (s.count() == 0) throw std::runtime_error("shp has no frames");
        // Optional trailing frame index (derivation aid for the Combat Phase's
        // fire/die sequence dumps -- Task 7): defaults to 0 (prior behavior
        // unchanged for every existing caller/test).
        int frame = 0;
        if (argc >= 6) frame = std::atoi(argv[5]);
        if (frame < 0 || frame >= s.count()) throw std::runtime_error("frame out of range");
        auto praw=read_path(m, argv[3]);
        Palette pal=Palette::load(praw.data(),praw.size());
        auto idx=s.decode_indexed(frame);
        auto rgba=ShpFile::palettize(idx,pal);
        auto fi=s.frame_info(frame);
        stbi_write_png(argv[4], fi.w, fi.h, 4, rgba.data(), fi.w*4);
        std::cout<<"wrote "<<argv[4]<<" ("<<fi.w<<"x"<<fi.h<<", frame "<<frame<<"/"<<s.count()<<")\n";
        return 0;
    } catch (const std::exception& ex){
        std::cerr<<"error: "<<ex.what()<<"\n";
        return 1;
    }
}

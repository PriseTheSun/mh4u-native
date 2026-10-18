#include "texture_pack.hpp"
#include "image.hpp"
#include "catalog.hpp"
#include <picosha2.h>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mh4u {
namespace {
constexpr std::size_t mib=1024U*1024U;
[[noreturn]] void fail(const std::string& why) { throw std::runtime_error("Pack de texturas: "+why); }
std::vector<std::uint8_t> read(const std::filesystem::path& path,std::size_t limit) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)fail("arquivo indisponível");
    const auto end=file.tellg();
    if(end<0||std::uint64_t(end)>limit)fail("arquivo excede orçamento");
    std::vector<std::uint8_t> data(static_cast<std::size_t>(end));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(data.size()))||file.peek()!=std::char_traits<char>::eof())fail("arquivo mudou durante a leitura");
    return data;
}
std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> out;
    for(std::size_t first=0;;) {
        const auto tab=line.find('\t',first);out.push_back(line.substr(first,tab-first));
        if(tab==std::string::npos)return out;
        first=tab+1;
    }
}
bool digest(const std::string& s) {
    return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
bool utf8(const std::string& s) {
    for(std::size_t i=0;i<s.size();) {
        const auto lead=static_cast<unsigned char>(s[i++]);
        if(lead<128)continue;
        unsigned count=0,value=0,minimum=0;
        if(lead>=0xc2&&lead<=0xdf){count=1;value=lead&31;minimum=0x80;}
        else if(lead>=0xe0&&lead<=0xef){count=2;value=lead&15;minimum=0x800;}
        else if(lead>=0xf0&&lead<=0xf4){count=3;value=lead&7;minimum=0x10000;}
        else return false;
        if(count>s.size()-i)return false;
        while(count--) {
            const auto next=static_cast<unsigned char>(s[i++]);
            if((next&0xc0)!=0x80)return false;
            value=(value<<6)|(next&63);
        }
        if(value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;
    }
    return true;
}
std::size_t size(const std::string& s) {
    std::size_t value=0;const auto [end,error]=std::from_chars(s.data(),s.data()+s.size(),value);
    if(error!=std::errc{}||end!=s.data()+s.size()||std::to_string(value)!=s||value==0||value>64*mib)fail("tamanho de PNG inválido");
    return value;
}
}
TexturePack load_texture_pack(const std::filesystem::path& manifest) {
    const auto data=read(manifest,1024*1024);
    const std::string text(data.begin(),data.end());
    if(text.empty()||text.back()!='\n'||text.find('\r')!=std::string::npos||text.find('\0')!=std::string::npos)fail("manifesto exige linhas LF completas");
    std::istringstream input(text);std::string line;
    if(!std::getline(input,line)||line!="MH4U-TEXTURE-PACK 1")fail("versão desconhecida");
    if(!std::getline(input,line))fail("nome ausente");
    auto name=fields(line);
    if(name.size()!=2||name[0]!="name"||name[1].empty()||name[1].size()>120||!utf8(name[1]))fail("nome inválido");
    for(unsigned char c:name[1])if(c<32||c==127)fail("controle no nome");
    TexturePack pack;pack.name=name[1];std::size_t total=0,encoded_total=0;
    const auto root=std::filesystem::canonical(manifest).parent_path();
    while(std::getline(input,line)) {
        const auto entry=fields(line);
        if(entry.size()!=5||entry[0]!="replace"||!digest(entry[1])||!digest(entry[2]))fail("registro de substituição inválido");
        if(pack.replacements.size()>=4096||pack.replacements.contains(entry[1]))fail("identidade repetida ou mais de 4096 substituições");
        const auto expected=size(entry[3]);
        if(expected>256*mib-encoded_total)fail("PNGs excedem orçamento agregado");
        encoded_total+=expected;
        if(!utf8(entry[4]))fail("caminho não é UTF-8");
        const auto path=catalog_path(root,entry[4]);
        if(path.empty())fail("caminho da substituição ausente");
        const auto encoded=read(path,expected);
        if(encoded.size()!=expected||picosha2::hash256_hex_string(encoded)!=entry[2])fail("identidade ou tamanho da substituição diverge");
        auto image=decode_png(encoded,256*mib-total);total+=image.rgb.size()+image.alpha.size();
        pack.replacements.emplace(entry[1],TextureReplacement{std::move(image),entry[2]});
    }
    if(pack.replacements.empty())fail("nenhuma substituição declarada");
    return pack;
}
}

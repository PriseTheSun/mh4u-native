#include "world_config.hpp"
#include "catalog.hpp"
#include <charconv>
#include <cmath>
#include <map>
#include <sstream>

namespace mh4u {
WorldConfig load_world_config(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);
    if(!std::filesystem::is_regular_file(path)||size>64U*1024U)
        throw std::runtime_error("Receita de cena ausente ou maior que 64 KiB");
    std::ifstream input(path,std::ios::binary);
    std::string text(static_cast<std::size_t>(size),'\0');
    input.read(text.data(),static_cast<std::streamsize>(text.size()));
    if(!input||input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Receita de cena mudou durante a leitura");
    if(text.empty()||text.back()!='\n'||text.find_first_of(std::string("\0\r",2))!=std::string::npos)
        throw std::runtime_error("Receita de cena exige linhas LF completas");
    const std::string header="MH4U-DIAGNOSTIC-WORLD 1\n";
    if(!text.starts_with(header))throw std::runtime_error("Versão de receita de cena não suportada");
    std::map<std::string,std::string> fields;
    for(std::size_t begin=header.size();begin<text.size();) {
        const auto end=text.find('\n',begin),tab=text.find('\t',begin);
        if(tab==std::string::npos||tab>=end||tab==begin||tab+1==end||text.find('\t',tab+1)<end)
            throw std::runtime_error("Campo de cena exige nome e valor separados por TAB");
        if(!fields.emplace(text.substr(begin,tab-begin),text.substr(tab+1,end-tab-1)).second)
            throw std::runtime_error("Campo duplicado na receita de cena");
        begin=end+1;
    }
    const auto take=[&](const char* key) {
        const auto found=fields.find(key);
        if(found==fields.end())throw std::runtime_error(std::string("Campo de cena ausente: ")+key);
        auto value=std::move(found->second);fields.erase(found);return value;
    };
    const auto number=[&](const char* key) {
        const auto value=take(key);double result{};
        const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
        if(error!=std::errc{}||end!=value.data()+value.size()||!std::isfinite(result))
            throw std::runtime_error(std::string("Número inválido na receita de cena: ")+key);
        return result;
    };
    const auto root=std::filesystem::canonical(path).parent_path();
    WorldConfig result;
    result.stage=catalog_path(root,take("stage"));result.actor=catalog_path(root,take("actor"));
    result.floor=catalog_path(root,take("floor"));result.walls=catalog_path(root,take("walls"));
    result.spawn={number("spawn-x"),number("spawn-y"),number("spawn-z")};
    result.heading=number("heading");result.radius=number("radius");result.height=number("height");
    result.speed=number("speed");result.min_normal_y=number("min-normal-y");
    result.actor_ground_offset=number("actor-ground-offset");
    if(!fields.empty())throw std::runtime_error("Campo desconhecido na receita de cena: "+fields.begin()->first);
    if(result.speed<=0||result.speed>10000)throw std::runtime_error("Velocidade diagnóstica fora de (0, 10000]");
    for(const auto* resource:{&result.stage,&result.actor,&result.floor,&result.walls})
        if(resource->empty()||!std::filesystem::is_regular_file(*resource))throw std::runtime_error("Recurso da cena ausente");
    return result;
}
bool WorldConfig::operator==(const WorldConfig& other) const {
    return stage==other.stage&&actor==other.actor&&floor==other.floor&&walls==other.walls&&
        spawn.x==other.spawn.x&&spawn.y==other.spawn.y&&spawn.z==other.spawn.z&&heading==other.heading&&
        radius==other.radius&&height==other.height&&speed==other.speed&&min_normal_y==other.min_normal_y&&
        actor_ground_offset==other.actor_ground_offset;
}
}

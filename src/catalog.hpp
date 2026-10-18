#pragma once
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace mh4u {
struct ResourceSpec {
    std::string label;
    std::filesystem::path model,texture,alpha,alternative,alternative_alpha;
};
inline std::filesystem::path catalog_path(const std::filesystem::path& root,const std::string& value) {
    if(value.empty()) return {};
    if(value.size()>4096||value.find_first_of(":\\")!=std::string::npos) throw std::runtime_error("Caminho inválido no catálogo");
    for(unsigned char c:value)if(c<32||c==127)throw std::runtime_error("Caractere de controle no caminho do catálogo");
    std::u8string utf8;for(unsigned char c:value)utf8.push_back(static_cast<char8_t>(c));
    const std::filesystem::path relative(utf8);
    if(relative.is_absolute()||relative.has_root_name())throw std::runtime_error("O catálogo exige caminhos relativos");
    for(const auto& part:relative)if(part==".."||part==".")throw std::runtime_error("O caminho do catálogo não pode conter . ou ..");
    const auto resolved=std::filesystem::weakly_canonical(root/relative);
    auto parent=root.begin(),target=resolved.begin();
    for(;parent!=root.end();++parent,++target)if(target==resolved.end()||*parent!=*target)throw std::runtime_error("Recurso fora da pasta do catálogo");
    return resolved;
}
inline std::vector<ResourceSpec> load_catalog(const std::filesystem::path& path) {
    constexpr std::uintmax_t limit=64*1024;
    const auto size=std::filesystem::file_size(path);
    if(!std::filesystem::is_regular_file(path)||size>limit)throw std::runtime_error("Catálogo ausente ou maior que 64 KiB");
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Não foi possível abrir o catálogo");
    std::string text(static_cast<std::size_t>(size),'\0');
    input.read(text.data(),static_cast<std::streamsize>(text.size()));
    if(!input||input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("O catálogo mudou durante a leitura");
    if(text.find('\0')!=std::string::npos||text.find('\r')!=std::string::npos||text.empty()||text.back()!='\n')throw std::runtime_error("Catálogo deve usar texto UTF-8 com linhas LF completas");
    const std::string header="MH4U-LAB-CATALOG 1\n";
    if(!text.starts_with(header))throw std::runtime_error("Versão de catálogo não suportada");
    const auto root=std::filesystem::canonical(path).parent_path();
    std::vector<ResourceSpec> result;std::set<std::string> labels;
    for(std::size_t start=header.size();start<text.size();) {
        const auto end=text.find('\n',start);const auto line=text.substr(start,end-start);start=end+1;
        std::vector<std::string> columns;
        for(std::size_t first=0;;) {const auto tab=line.find('\t',first);columns.push_back(line.substr(first,tab-first));if(tab==std::string::npos)break;first=tab+1;}
        if(columns.size()!=6)throw std::runtime_error("Linha de catálogo exige seis campos separados por TAB");
        if(columns[0].empty()||columns[0].size()>120||!labels.insert(columns[0]).second)throw std::runtime_error("Rótulo vazio, duplicado ou maior que 120 bytes");
        for(unsigned char c:columns[0])if(c<32||c==127)throw std::runtime_error("Caractere de controle no rótulo");
        if(columns[1].empty())throw std::runtime_error("Modelo ausente no catálogo");
        ResourceSpec spec{columns[0],catalog_path(root,columns[1]),catalog_path(root,columns[2]),catalog_path(root,columns[3]),catalog_path(root,columns[4]),catalog_path(root,columns[5])};
        if((!spec.alpha.empty()&&spec.texture.empty())||(!spec.alternative_alpha.empty()&&spec.alternative.empty()))throw std::runtime_error("Alpha no catálogo requer sua textura");
        result.push_back(std::move(spec));
        if(result.size()>64)throw std::runtime_error("Catálogo excede 64 recursos");
    }
    if(result.empty())throw std::runtime_error("Catálogo vazio");return result;
}
}

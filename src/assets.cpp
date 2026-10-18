#include "assets.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mh4u {
namespace {
constexpr std::size_t file_budget = 128U * 1024U * 1024U;
constexpr std::size_t image_budget = 64U * 1024U * 1024U;
constexpr std::size_t vertex_budget = 2U * 1024U * 1024U;
[[noreturn]] void fail(const std::string& reason) { throw std::runtime_error(reason); }
std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) fail("Não foi possível abrir: " + path.string());
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > file_budget)
        fail("Arquivo excede o limite de leitura de 128 MiB: " + path.string());
    std::string data(static_cast<std::size_t>(end), '\0');
    input.seekg(0);
    if (!input.read(data.data(), static_cast<std::streamsize>(data.size()))) fail("Leitura incompleta: " + path.string());
    return data;
}
int integer(std::string_view token) {
    int value{};
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error != std::errc{} || end != token.data() + token.size()) fail("Inteiro inválido no recurso");
    return value;
}
float scalar(std::string_view token) {
    float value{};
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error != std::errc{} || end != token.data() + token.size() || !std::isfinite(value))
        fail("Número inválido ou não finito no OBJ");
    return value;
}
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vec3 normal(Vec3 a) {
    const float length = std::hypot(a.x,a.y,a.z);
    if (!std::isfinite(length)) fail("Geometria excede a faixa numérica");
    if (length < 1e-12f) return {0,1,0};
    return {a.x/length,a.y/length,a.z/length};
}
std::size_t index(std::string_view token, std::size_t count) {
    const int parsed = integer(token);
    const std::int64_t resolved = parsed > 0 ? std::int64_t(parsed)-1 : std::int64_t(count)+parsed;
    if (parsed == 0 || resolved < 0 || static_cast<std::uint64_t>(resolved) >= count)
        fail("Índice OBJ fora do intervalo");
    return static_cast<std::size_t>(resolved);
}
bool space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='\v' || c=='\f'; }
class Tokens {
public:
    explicit Tokens(const std::string& data) : data_(data) {}
    std::string_view next() {
        while (position < data_.size()) {
            if (space(data_[position])) { ++position; continue; }
            if (data_[position] == '#') {
                while (position < data_.size() && data_[position] != '\n') ++position;
                continue;
            }
            break;
        }
        const auto start = position;
        while (position < data_.size() && !space(data_[position]) && data_[position]!='#') ++position;
        if (start == position) fail("Cabeçalho ou pixels PPM incompletos");
        return std::string_view(data_).substr(start,position-start);
    }
    std::size_t position{};
private:
    const std::string& data_;
};
}

Mesh load_obj(const std::filesystem::path& path) {
    const std::string source = read_file(path);
    std::istringstream input(source);
    std::vector<Vec3> positions, normals;
    std::vector<Vec2> uvs;
    Mesh mesh;
    std::string line;
    std::size_t line_number = 0;
    try {
        while (std::getline(input,line)) {
            ++line_number;
            if (line.size() > 65536) fail("Linha OBJ excede o limite de 64 KiB");
            std::istringstream words(line);
            std::vector<std::string> tokens;
            std::string token;
            while (words >> token) {
                if (token.starts_with('#')) break;
                if (tokens.size() >= 64) fail("Registro OBJ com excesso de campos");
                tokens.push_back(token);
            }
            if (tokens.empty()) continue;
            if (tokens[0] == "v" || tokens[0] == "vn") {
                if (tokens.size()!=4) fail("Posições/normais OBJ devem ter três componentes");
                auto& target = tokens[0]=="v" ? positions : normals;
                if (target.size() >= vertex_budget) fail("OBJ excede o limite de vértices");
                target.push_back({scalar(tokens[1]),scalar(tokens[2]),scalar(tokens[3])});
            } else if (tokens[0] == "vt") {
                if (tokens.size()!=3) fail("UV OBJ deve ter duas componentes");
                if (uvs.size() >= vertex_budget) fail("OBJ excede o limite de UVs");
                uvs.push_back({scalar(tokens[1]),scalar(tokens[2])});
            } else if (tokens[0] == "usemtl") {
                ++mesh.material_groups;
            } else if (tokens[0] == "f") {
                if (tokens.size()!=4) fail("O viewer requer OBJ triangulado; face não triangular encontrada");
                if (mesh.triangles.size() > vertex_budget-3) fail("OBJ excede o orçamento de triângulos expandidos");
                std::array<Vertex,3> face{};
                std::array<bool,3> has_normal{};
                for (std::size_t i=0;i<3;++i) {
                    const auto& part = tokens[i+1];
                    const auto first=part.find('/');
                    const auto second=first==std::string::npos ? first : part.find('/',first+1);
                    if (second!=std::string::npos && part.find('/',second+1)!=std::string::npos) fail("Índice OBJ com separadores extras");
                    face[i].position = positions[index(std::string_view(part).substr(0,first),positions.size())];
                    if (first!=std::string::npos) {
                        const auto uv=std::string_view(part).substr(first+1,second==std::string::npos ? second : second-first-1);
                        if (!uv.empty()) { face[i].uv=uvs[index(uv,uvs.size())]; mesh.has_uv=true; }
                    }
                    if (second!=std::string::npos) {
                        const auto n=std::string_view(part).substr(second+1);
                        if (n.empty()) fail("Índice de normal OBJ vazio");
                        face[i].normal=normal(normals[index(n,normals.size())]); has_normal[i]=true;
                    }
                }
                const auto generated=normal(cross(subtract(face[1].position,face[0].position),subtract(face[2].position,face[0].position)));
                for (std::size_t i=0;i<3;++i) { if (!has_normal[i]) face[i].normal=generated; mesh.triangles.push_back(face[i]); }
            } else if (tokens[0]!="o" && tokens[0]!="g" && tokens[0]!="s" && tokens[0]!="mtllib") {
                fail("Registro OBJ não suportado: " + tokens[0]);
            }
        }
    } catch (const std::exception& e) { fail(path.string()+":"+std::to_string(line_number)+": "+e.what()); }
    if (mesh.triangles.empty()) fail("OBJ sem triângulos: " + path.string());
    mesh.minimum=mesh.maximum=mesh.triangles.front().position;
    for (const auto& vertex : mesh.triangles) {
        const auto p=vertex.position;
        mesh.minimum={std::min(mesh.minimum.x,p.x),std::min(mesh.minimum.y,p.y),std::min(mesh.minimum.z,p.z)};
        mesh.maximum={std::max(mesh.maximum.x,p.x),std::max(mesh.maximum.y,p.y),std::max(mesh.maximum.z,p.z)};
    }
    mesh.center={mesh.minimum.x*0.5f+mesh.maximum.x*0.5f,mesh.minimum.y*0.5f+mesh.maximum.y*0.5f,mesh.minimum.z*0.5f+mesh.maximum.z*0.5f};
    const auto diagonal=subtract(mesh.maximum,mesh.minimum);
    mesh.radius=std::hypot(diagonal.x,diagonal.y,diagonal.z)*0.5f;
    if (!std::isfinite(mesh.radius) || mesh.radius < 1e-8f) fail("Extensão geométrica inválida no OBJ");
    mesh.source_positions=positions.size();
    return mesh;
}

namespace {
struct Raster { int width{}, height{}, channels{}; std::vector<std::uint8_t> pixels; };
Raster load_pnm(const std::filesystem::path& path) {
    const auto source=read_file(path);
    Tokens tokens(source);
    const auto format=tokens.next();
    if (format!="P6" && format!="P3" && format!="P5" && format!="P2") fail("Imagem deve ser PPM P6/P3 ou PGM P5/P2");
    Raster image;
    image.channels=(format=="P6" || format=="P3") ? 3 : 1;
    image.width=integer(tokens.next()); image.height=integer(tokens.next());
    const int maximum=integer(tokens.next());
    if (image.width<=0 || image.height<=0 || maximum<=0 || maximum>255) fail("Dimensão ou profundidade PPM não suportada");
    const auto pixels=static_cast<std::uint64_t>(image.width)*static_cast<std::uint64_t>(image.height);
    if (pixels > image_budget/image.channels) fail("Textura excede o limite de 64 MiB decodificados");
    const auto count=static_cast<std::size_t>(pixels)*image.channels;
    image.pixels.resize(count);
    if (format=="P6" || format=="P5") {
        auto position=tokens.position;
        if (position>=source.size() || !space(source[position])) fail("Separador PPM ausente");
        const char separator=source[position++];
        if (separator=='\r' && position<source.size() && source[position]=='\n') ++position;
        if (source.size()-position!=count) fail("Pixels PPM incompletos ou dados extras após a imagem");
        for (std::size_t i=0;i<count;++i) {
            const auto value=static_cast<unsigned char>(source[position+i]);
            if (value>maximum) fail("Pixel PPM excede a profundidade declarada");
            image.pixels[i]=static_cast<std::uint8_t>(value*255/maximum);
        }
    } else {
        for (auto& value:image.pixels) {
            const int parsed=integer(tokens.next());
            if (parsed<0 || parsed>maximum) fail("Pixel PPM fora do intervalo");
            value=static_cast<std::uint8_t>(parsed*255/maximum);
        }
        while (tokens.position<source.size() && space(source[tokens.position])) ++tokens.position;
        if (tokens.position!=source.size()) fail("Dados extras após os pixels PPM");
    }
    return image;
}
}

Image load_ppm(const std::filesystem::path& path) {
    auto raster=load_pnm(path);
    if(raster.channels!=3) fail("A textura de cor deve ser PPM RGB");
    return {raster.width,raster.height,std::move(raster.pixels),{}};
}

void apply_pgm_alpha(Image& image,const std::filesystem::path& path) {
    auto raster=load_pnm(path);
    if(raster.channels!=1 || raster.width!=image.width || raster.height!=image.height)
        fail("Alpha deve ser PGM e ter as mesmas dimensões da textura");
    image.alpha=std::move(raster.pixels);
}

void write_ppm(const std::filesystem::path& path, const Image& image) {
    if (image.width<=0 || image.height<=0 || image.rgb.size()!=std::uint64_t(image.width)*std::uint64_t(image.height)*3)
        fail("Imagem inconsistente ao salvar PPM");
    std::ofstream output(path,std::ios::binary);
    if (!output) fail("Não foi possível salvar captura: " + path.string());
    output << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    output.write(reinterpret_cast<const char*>(image.rgb.data()),static_cast<std::streamsize>(image.rgb.size()));
    if (!output) fail("Falha ao gravar captura: " + path.string());
}
}  // namespace mh4u

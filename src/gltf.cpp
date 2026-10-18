#include "scene.hpp"
#include "image.hpp"
#include <picosha2.h>
#include <cgltf.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace mh4u {
namespace {
constexpr std::size_t mib=1024U*1024U;
[[noreturn]] void fail(const std::string& reason) { throw std::runtime_error("glTF: "+reason); }
std::vector<std::uint8_t> bytes(const std::filesystem::path& path,std::size_t limit=128*mib) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) fail("não foi possível abrir "+path.string());
    const auto end=input.tellg();
    if(end<0||static_cast<std::uint64_t>(end)>limit) fail("arquivo excede orçamento de leitura");
    std::vector<std::uint8_t> result(static_cast<std::size_t>(end)); input.seekg(0);
    if(!input.read(reinterpret_cast<char*>(result.data()),static_cast<std::streamsize>(result.size()))) fail("leitura incompleta");
    return result;
}
std::filesystem::path relative_uri(const std::filesystem::path& directory,const char* uri) {
    if(!uri) fail("URI externa ausente");
    std::string decoded=uri; const auto decoded_size=cgltf_decode_uri(decoded.data()); decoded.resize(decoded_size);
    if(decoded.find('\0')!=std::string::npos) fail("URI contém byte nulo");
    if(decoded.empty()||decoded.find(':')!=std::string::npos||decoded.find('\\')!=std::string::npos||decoded.find('?')!=std::string::npos||decoded.find('#')!=std::string::npos) fail("somente URIs relativas locais são suportadas");
    std::u8string utf; for(unsigned char c:decoded) utf.push_back(static_cast<char8_t>(c));
    const auto relative=std::filesystem::path(utf);
    if(relative.is_absolute()) fail("URI absoluta não suportada");
    for(const auto& component:relative) if(component=="..") fail("URI não pode sair da pasta da cena");
    return directory/relative;
}
struct AllocationBudget { std::size_t used=0; };
struct alignas(std::max_align_t) Allocation { std::size_t size; };
void* allocate(void* opaque,cgltf_size size) {
    auto& budget=*static_cast<AllocationBudget*>(opaque);
    if(size>128*mib-budget.used) return nullptr;
    auto* block=static_cast<Allocation*>(std::malloc(sizeof(Allocation)+size));
    if(!block) return nullptr;
    block->size=size; budget.used+=size; return block+1;
}
void deallocate(void* opaque,void* pointer) {
    if(!pointer) return;
    auto* block=static_cast<Allocation*>(pointer)-1;
    static_cast<AllocationBudget*>(opaque)->used-=block->size; std::free(block);
}
void expect(const cgltf_accessor* accessor,cgltf_type type,std::size_t count=0,bool floating=true) {
    if(!accessor||accessor->type!=type||(count&&accessor->count!=count)||(floating&&accessor->component_type!=cgltf_component_type_r_32f)) fail("tipo ou contagem de accessor não suportado");
}
template<std::size_t N> std::array<float,N> floats(const cgltf_accessor* accessor,std::size_t index) {
    std::array<float,N> value{};
    if(!cgltf_accessor_read_float(accessor,index,value.data(),N)) fail("falha ao ler accessor");
    for(float component:value) if(!std::isfinite(component)) fail("accessor contém número não finito");
    return value;
}
Vec3 vector3(const cgltf_accessor* a,std::size_t i) { const auto v=floats<3>(a,i); return {v[0],v[1],v[2]}; }
WrapMode wrap(cgltf_wrap_mode mode) {
    if(mode==cgltf_wrap_mode_repeat) return WrapMode::repeat;
    if(mode==cgltf_wrap_mode_clamp_to_edge) return WrapMode::clamp;
    if(mode==cgltf_wrap_mode_mirrored_repeat) return WrapMode::mirror;
    fail("modo de wrap não suportado");
}
}
Scene load_gltf(const std::filesystem::path& path) {
    AllocationBudget budget; cgltf_options options{};
    options.memory={allocate,deallocate,&budget};
    const auto file=bytes(path); cgltf_data* raw=nullptr;
    if(cgltf_parse(&options,file.data(),file.size(),&raw)!=cgltf_result_success) fail("documento inválido ou orçamento do parser excedido");
    std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
    if(!data->asset.version||std::strcmp(data->asset.version,"2.0")) fail("somente glTF 2.0");
    if(data->extensions_required_count) fail("extensões obrigatórias não implementadas");
    if(data->nodes_count>65536||data->meshes_count>4096||data->materials_count>4096||data->animations_count>4096||data->images_count>4096) fail("cena excede limite de estruturas");
    std::vector<std::vector<std::uint8_t>> buffers(data->buffers_count);
    std::size_t buffer_total=0;
    for(std::size_t i=0;i<data->buffers_count;++i) {
        auto& buffer=data->buffers[i];
        if(buffer.size>256*mib-buffer_total) fail("buffers excedem orçamento total de 256 MiB");
        buffer_total+=buffer.size;
        if(buffer.uri) {
            buffers[i]=bytes(relative_uri(path.parent_path(),buffer.uri),buffer.size);
            if(buffers[i].size()<buffer.size) fail("buffer externo truncado");
            buffer.data=buffers[i].data();
        } else {
            if(i!=0||data->bin_size<buffer.size||!data->bin) fail("buffer GLB ausente ou truncado");
            buffer.data=const_cast<void*>(data->bin);
        }
        buffer.data_free_method=cgltf_data_free_method_none;
    }
    // Check arithmetic before cgltf_validate, whose accessor arithmetic assumes valid sizes.
    for(std::size_t i=0;i<data->buffer_views_count;++i) {
        const auto& view=data->buffer_views[i];
        if(view.has_meshopt_compression||!view.buffer) fail("bufferView comprimido ou sem buffer");
        if(view.offset>view.buffer->size||view.size>view.buffer->size-view.offset) fail("bufferView fora do buffer");
    }
    for(std::size_t i=0;i<data->accessors_count;++i) {
        const auto& a=data->accessors[i];
        if(a.is_sparse||!a.buffer_view) fail("accessor sparse ou implícito não implementado");
        const auto element=cgltf_calc_size(a.type,a.component_type),component=cgltf_component_size(a.component_type);
        if(!element||!component||!a.count||a.count>2*mib||a.stride<element||a.stride>256||a.offset>a.buffer_view->size||element>a.buffer_view->size-a.offset) fail("dimensões de accessor inválidas");
        if(a.count-1>(a.buffer_view->size-a.offset-element)/a.stride) fail("accessor fora do bufferView");
        if((a.offset+a.buffer_view->offset)%component||a.stride%component) fail("accessor desalinhado");
    }
    if(cgltf_validate(data.get())!=cgltf_result_success) fail("validação estrutural falhou");
    Scene scene;
    scene.notes.push_back("Materiais: cor base e iluminação de inspeção; shader original não reproduzido.");
    scene.nodes.resize(data->nodes_count);
    for(std::size_t i=0;i<data->nodes_count;++i) {
        const auto& in=data->nodes[i]; auto& out=scene.nodes[i];
        if(in.has_mesh_gpu_instancing||in.weights_count) fail("instancing ou morph targets não implementados");
        out.name=in.name?in.name:("node_"+std::to_string(i));
        out.parent=in.parent?static_cast<int>(cgltf_node_index(data.get(),in.parent)):-1;
        out.has_matrix=in.has_matrix;
        if(in.has_matrix&&(in.has_translation||in.has_rotation||in.has_scale)) fail("nó contém matrix e TRS simultaneamente");
        if(in.has_matrix) std::copy_n(in.matrix,16,out.matrix.begin());
        out.rest.translation={in.translation[0],in.translation[1],in.translation[2]};
        std::copy_n(in.rotation,4,out.rest.rotation.begin());
        out.rest.scale={in.scale[0],in.scale[1],in.scale[2]};
    }
    for(std::size_t i=0;i<data->skins_count;++i) {
        const auto& in=data->skins[i]; Skin out;
        if(in.joints_count>65536) fail("skin excede limite de juntas");
        if(in.inverse_bind_matrices) expect(in.inverse_bind_matrices,cgltf_type_mat4,in.joints_count);
        for(std::size_t j=0;j<in.joints_count;++j) {
            out.joints.push_back(cgltf_node_index(data.get(),in.joints[j]));
            out.inverse_bind.push_back(in.inverse_bind_matrices?floats<16>(in.inverse_bind_matrices,j):identity_matrix());
        }
        scene.skins.push_back(std::move(out));
    }
    std::size_t image_total=0;
    for(std::size_t i=0;i<data->images_count;++i) {
        const auto& in=data->images[i]; std::vector<std::uint8_t> encoded;
        if(in.uri) encoded=bytes(relative_uri(path.parent_path(),in.uri),64*mib);
        else if(in.buffer_view) {
            const auto* start=cgltf_buffer_view_data(in.buffer_view);
            if(!start||in.buffer_view->size>64*mib) fail("bufferView de imagem inválido");
            encoded.assign(start,start+in.buffer_view->size);
        } else fail("imagem sem URI nem bufferView");
        auto image=decode_png(encoded,256*mib-image_total); image_total+=image.rgb.size()+image.alpha.size();
        if(image_total>256*mib) fail("imagens excedem 256 MiB");
        scene.images.push_back({std::move(image),picosha2::hash256_hex_string(encoded)});
    }
    for(std::size_t i=0;i<data->materials_count;++i) {
        const auto& in=data->materials[i]; Material out;
        out.name=in.name?in.name:("material_"+std::to_string(i));
        if(in.alpha_mode==cgltf_alpha_mode_blend) fail("material BLEND ainda não implementado (exige ordenação transparente)");
        out.alpha=in.alpha_mode==cgltf_alpha_mode_mask?AlphaMode::mask:AlphaMode::opaque;
        out.alpha_cutoff=in.alpha_cutoff; out.double_sided=in.double_sided;
        if(in.has_pbr_metallic_roughness) {
            const auto& pbr=in.pbr_metallic_roughness; std::copy_n(pbr.base_color_factor,4,out.color.begin());
            const auto& view=pbr.base_color_texture;
            if(view.texture) {
                if(view.texcoord!=0||view.has_transform||!view.texture->image) fail("baseColorTexture exige UV0 e PNG sem transformações extras");
                out.image=static_cast<int>(cgltf_image_index(data.get(),view.texture->image));
                if(view.texture->sampler) {
                    const auto& sampler=*view.texture->sampler; out.wrap_s=wrap(sampler.wrap_s); out.wrap_t=wrap(sampler.wrap_t);
                    out.min_linear=sampler.min_filter!=cgltf_filter_type_nearest;
                    out.mag_linear=sampler.mag_filter!=cgltf_filter_type_nearest;
                    if(sampler.min_filter!=cgltf_filter_type_undefined&&sampler.min_filter!=cgltf_filter_type_nearest&&sampler.min_filter!=cgltf_filter_type_linear)
                        fail("filtragem mipmap ainda não implementada");
                }
            }
        }
        for(float value:out.color) if(!std::isfinite(value)||value<0||value>1) fail("baseColorFactor inválido");
        if(!std::isfinite(out.alpha_cutoff)||out.alpha_cutoff<0) fail("alphaCutoff inválido");
        scene.materials.push_back(out);
    }
    std::vector<std::size_t> mesh_start; std::size_t vertex_total=0,index_total=0;
    for(std::size_t m=0;m<data->meshes_count;++m) {
        mesh_start.push_back(scene.primitives.size());
        const auto& mesh=data->meshes[m];
        for(std::size_t p=0;p<mesh.primitives_count;++p) {
            const auto& in=mesh.primitives[p]; Primitive out;
            if(in.type!=cgltf_primitive_type_triangles||in.targets_count||in.has_draco_mesh_compression) fail("somente TRIANGLES sem morph/compressão");
            const auto* positions=cgltf_find_accessor(&in,cgltf_attribute_type_position,0);
            const auto* normals=cgltf_find_accessor(&in,cgltf_attribute_type_normal,0);
            const auto* uv=cgltf_find_accessor(&in,cgltf_attribute_type_texcoord,0);
            const auto* joints=cgltf_find_accessor(&in,cgltf_attribute_type_joints,0);
            const auto* weights=cgltf_find_accessor(&in,cgltf_attribute_type_weights,0);
            expect(positions,cgltf_type_vec3); expect(normals,cgltf_type_vec3,positions->count);
            if(uv) expect(uv,cgltf_type_vec2,positions->count);
            if(bool(joints)!=bool(weights)) fail("JOINTS_0 e WEIGHTS_0 precisam coexistir");
            for(std::size_t a=0;a<in.attributes_count;++a)
                if((in.attributes[a].type==cgltf_attribute_type_joints||in.attributes[a].type==cgltf_attribute_type_weights)&&in.attributes[a].index!=0) fail("mais de quatro pesos por vértice não implementado");
            out.skinned=joints!=nullptr; out.material=in.material?static_cast<int>(cgltf_material_index(data.get(),in.material)):-1;
            if(out.material>=0&&scene.materials[out.material].image>=0&&!uv) fail("primitiva texturizada sem UV0");
            if(joints) {
                expect(joints,cgltf_type_vec4,positions->count,false); expect(weights,cgltf_type_vec4,positions->count);
                if(joints->normalized||(joints->component_type!=cgltf_component_type_r_8u&&joints->component_type!=cgltf_component_type_r_16u)) fail("JOINTS_0 deve ser u8/u16 não normalizado");
            }
            vertex_total+=positions->count; if(vertex_total>2*mib) fail("orçamento de vértices excedido");
            for(std::size_t v=0;v<positions->count;++v) {
                SkinVertex vertex; vertex.vertex.position=vector3(positions,v); vertex.vertex.normal=vector3(normals,v);
                if(uv) { const auto t=floats<2>(uv,v); vertex.vertex.uv={t[0],t[1]}; }
                if(joints) {
                    if(!cgltf_accessor_read_uint(joints,v,vertex.joints.data(),4)) fail("juntas ilegíveis");
                    vertex.weights=floats<4>(weights,v);
                }
                out.vertices.push_back(vertex);
            }
            const auto count=in.indices?in.indices->count:positions->count;
            index_total+=count; if(index_total>2*mib) fail("orçamento de índices excedido");
            if(in.indices) {
                expect(in.indices,cgltf_type_scalar,0,false);
                const auto component=in.indices->component_type;
                if(in.indices->normalized||(component!=cgltf_component_type_r_8u&&component!=cgltf_component_type_r_16u&&component!=cgltf_component_type_r_32u))
                    fail("índices devem ser u8/u16/u32 não normalizados");
            }
            for(std::size_t index=0;index<count;++index) {
                const auto value=in.indices?cgltf_accessor_read_index(in.indices,index):index;
                if(value>=positions->count) fail("índice fora dos vértices");
                out.indices.push_back(static_cast<std::uint32_t>(value));
            }
            scene.primitives.push_back(std::move(out));
        }
    }
    std::vector<bool> visible(data->nodes_count,false); std::vector<std::size_t> pending;
    const auto* selected=data->scene?data->scene:(data->scenes_count?&data->scenes[0]:nullptr);
    if(selected) for(std::size_t i=0;i<selected->nodes_count;++i) pending.push_back(cgltf_node_index(data.get(),selected->nodes[i]));
    else for(std::size_t i=0;i<data->nodes_count;++i) if(!data->nodes[i].parent) pending.push_back(i);
    while(!pending.empty()) {
        const auto index=pending.back(); pending.pop_back(); if(visible[index]) continue; visible[index]=true;
        const auto& node=data->nodes[index];
        for(std::size_t i=0;i<node.children_count;++i) pending.push_back(cgltf_node_index(data.get(),node.children[i]));
        if(node.mesh) {
            const auto first=mesh_start[cgltf_mesh_index(data.get(),node.mesh)];
            for(std::size_t i=0;i<node.mesh->primitives_count;++i)
                scene.instances.push_back({first+i,index,node.skin?static_cast<int>(cgltf_skin_index(data.get(),node.skin)):-1});
        }
    }
    std::size_t output_rows=0,channels=0;
    for(std::size_t a=0;a<data->animations_count;++a) {
        const auto& in=data->animations[a]; Animation animation;
        animation.name=in.name?in.name:("animation_"+std::to_string(a));
        channels+=in.channels_count; if(channels>65536) fail("limite de canais excedido");
        for(std::size_t c=0;c<in.channels_count;++c) {
            const auto& source=in.channels[c]; AnimationChannel channel;
            if(!source.target_node||!source.sampler) fail("canal sem alvo/sampler");
            channel.node=cgltf_node_index(data.get(),source.target_node);
            if(source.target_path==cgltf_animation_path_type_translation) channel.path=ChannelPath::translation;
            else if(source.target_path==cgltf_animation_path_type_rotation) channel.path=ChannelPath::rotation;
            else if(source.target_path==cgltf_animation_path_type_scale) channel.path=ChannelPath::scale;
            else fail("canal não TRS ainda não implementado");
            const auto& sampler=*source.sampler;
            if(sampler.interpolation==cgltf_interpolation_type_linear) channel.interpolation=Interpolation::linear;
            else if(sampler.interpolation==cgltf_interpolation_type_step) channel.interpolation=Interpolation::step;
            else if(sampler.interpolation==cgltf_interpolation_type_cubic_spline) channel.interpolation=Interpolation::cubic_spline;
            else fail("interpolação não suportada");
            expect(sampler.input,cgltf_type_scalar);
            const bool cubic=channel.interpolation==Interpolation::cubic_spline;
            const std::size_t rows_per_key=cubic?3:1;
            if((cubic&&sampler.input->count<2)||sampler.input->count>(2*mib-output_rows)/rows_per_key)
                fail("quantidade de keyframes/tangentes inválida ou acima do orçamento");
            const auto rows=sampler.input->count*rows_per_key; output_rows+=rows;
            expect(sampler.output,channel.path==ChannelPath::rotation?cgltf_type_vec4:cgltf_type_vec3,rows);
            const auto value_at=[&](std::size_t row) {
                if(channel.path==ChannelPath::rotation) return floats<4>(sampler.output,row);
                const auto v=floats<3>(sampler.output,row); return std::array<float,4>{v[0],v[1],v[2],0};
            };
            channel.times.reserve(sampler.input->count);channel.values.reserve(sampler.input->count);
            if(cubic) {channel.in_tangents.reserve(sampler.input->count);channel.out_tangents.reserve(sampler.input->count);}
            for(std::size_t k=0;k<sampler.input->count;++k) {
                channel.times.push_back(floats<1>(sampler.input,k)[0]);
                channel.values.push_back(value_at(k*rows_per_key+(cubic?1:0)));
                if(cubic) {
                    channel.in_tangents.push_back(value_at(k*3));
                    channel.out_tangents.push_back(value_at(k*3+2));
                }
            }
            animation.channels.push_back(std::move(channel));
        }
        scene.animations.push_back(std::move(animation));
    }
    prepare_scene(scene); return scene;
}
}

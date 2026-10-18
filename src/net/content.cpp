#include "content.hpp"
#include "native_actor.hpp"
#include <cgltf.h>
#include <picosha2.h>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

namespace mh4u::net {
namespace {
[[noreturn]] void fail(const std::string& why) { throw std::runtime_error("manifest: "+why); }
std::uint64_t decimal(const std::string& text) {
    std::uint64_t value=0; const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(error!=std::errc{}||end!=text.data()+text.size()||std::to_string(value)!=text) fail("non-canonical decimal"); return value;
}
std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> result; std::size_t start=0;
    for(;;) { const auto end=line.find('\t',start); result.push_back(line.substr(start,end==std::string::npos?end:end-start)); if(end==std::string::npos) break; start=end+1; }
    return result;
}
std::filesystem::path local_path(const std::filesystem::path& root,const std::string& name) {
    if(name.empty()||name.size()>1024||name.find_first_of("\t\r\n\\:")!=std::string::npos||name.find('\0')!=std::string::npos) fail("invalid local relative path");
    const auto path=utf8_path(name);
    if(path.is_absolute()) fail("absolute resource path");
    for(const auto& part:path) if(part==".."||part==".") fail("non-canonical relative resource path");
    const auto resolved=std::filesystem::canonical(root/path),base=std::filesystem::canonical(root);
    auto a=base.begin(),b=resolved.begin();
    while(a!=base.end()&&b!=resolved.end()&&*a==*b) { ++a; ++b; }
    if(a!=base.end()||b==resolved.end()) fail("resource escapes manifest directory");
    return resolved;
}
std::string read(const std::filesystem::path& path,std::size_t max) {
    std::ifstream file(path,std::ios::binary|std::ios::ate); if(!file) fail("file cannot be opened");
    const auto size=file.tellg(); if(size<0||std::uint64_t(size)>max) fail("file exceeds byte budget");
    std::string bytes(static_cast<std::size_t>(size),'\0'); file.seekg(0); if(!file.read(bytes.data(),bytes.size())) fail("short read"); return bytes;
}
std::string hash_file(const std::filesystem::path& path,std::uint64_t expected) {
    std::ifstream file(path,std::ios::binary|std::ios::ate); if(!file||file.tellg()<0||std::uint64_t(file.tellg())!=expected) fail("resource size mismatch"); file.seekg(0);
    picosha2::hash256_one_by_one hash; std::array<char,65536> block{}; std::uint64_t total=0;
    while(file) { file.read(block.data(),block.size()); const auto size=file.gcount(); if(size<0||std::uint64_t(size)>expected-total) fail("resource changed during hashing"); total+=std::uint64_t(size); hash.process(block.begin(),block.begin()+size); }
    if(!file.eof()||total!=expected) fail("resource hashing failed"); hash.finish(); return picosha2::get_hash_hex_string(hash);
}
std::string uri_name(const char* source) {
    if(!source) fail("external buffer/image URI required");
    std::string decoded=source; decoded.resize(cgltf_decode_uri(decoded.data()));
    if(decoded.find_first_of("?#")!=std::string::npos) fail("URI query/fragment unsupported"); return decoded;
}
struct SceneDescription { std::set<std::string> references; std::vector<std::string> clips; };
SceneDescription describe_scene(const std::filesystem::path& root,const std::string& scene_name,bool world=false) {
    const auto scene_bytes=read(local_path(root,scene_name),8*1024*1024);
    cgltf_options options{}; cgltf_data* raw=nullptr;
    if(cgltf_parse(&options,scene_bytes.data(),scene_bytes.size(),&raw)!=cgltf_result_success) fail("scene glTF invalid");
    std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
    if(data->file_type!=cgltf_file_type_gltf||!data->asset.version||std::string(data->asset.version)!="2.0"||data->extensions_required_count) fail("manifest v1 supports external core glTF 2.0 only");
    if(data->animations_count>4096) fail("clip count exceeds 4096");
    std::vector<std::string> clips;
    for(std::size_t i=0;i<data->animations_count;++i) {
        const std::string name=data->animations[i].name?data->animations[i].name:("animation_"+std::to_string(i));
        if(name.empty()||name.size()>512||name.find_first_of("\r\n\t")!=std::string::npos) fail("invalid clip name");
        clips.push_back(name);
    }
    if(world&&(data->buffers_count>255||data->images_count>255-data->buffers_count)) fail("too many glTF dependencies");
    std::set<std::string> references{scene_name}; const auto parent=utf8_path(scene_name).parent_path();
    auto dependency=[&](const char* uri) {
        const auto name=uri_name(uri); const auto relative=(parent/utf8_path(name)).generic_u8string();
        const std::string text(relative.begin(),relative.end()); local_path(root,text); references.insert(text);
    };
    for(std::size_t i=0;i<data->buffers_count;++i) dependency(data->buffers[i].uri);
    for(std::size_t i=0;i<data->images_count;++i) if(data->images[i].uri) dependency(data->images[i].uri); else if(!data->images[i].buffer_view) fail("image has no resource");
    if(references.size()>256) fail("more than 256 resource files");
    return {std::move(references),std::move(clips)};
}

}
std::filesystem::path utf8_path(const std::string& text) {
    std::u8string result; for(unsigned char c:text) result.push_back(static_cast<char8_t>(c)); return std::filesystem::path(result);
}
Content verify_manifest(const std::filesystem::path& manifest) {
    const auto document=read(manifest,1024*1024);
    if(document.empty()||document.back()!='\n'||document.find('\r')!=std::string::npos||document.find('\0')!=std::string::npos) fail("requires canonical UTF-8/LF text");
    std::istringstream stream(document); std::string line;
    if(!std::getline(stream,line)||line!="MH4U-LAB-MANIFEST 1") fail("unsupported manifest version");
    if(!std::getline(stream,line)) fail("missing scene"); auto scene_field=fields(line);
    if(scene_field.size()!=2||scene_field[0]!="scene") fail("invalid scene record"); const auto scene_name=scene_field[1];
    if(!std::getline(stream,line)) fail("missing clip count"); const auto clip_field=fields(line);
    if(clip_field.size()!=2||clip_field[0]!="clips") fail("invalid clips record"); const auto count=decimal(clip_field[1]);
    if(count>4096) fail("clip count exceeds 4096"); Content result;
    for(std::uint64_t i=0;i<count;++i) {
        if(!std::getline(stream,line)) fail("missing clip"); const auto entry=fields(line);
        if(entry.size()!=3||entry[0]!="clip"||decimal(entry[1])!=i||entry[2].empty()||entry[2].size()>512) fail("invalid ordered clip record"); result.clips.push_back(entry[2]);
    }
    struct File { std::string hash; std::uint64_t size; };
    std::map<std::string,File> files; std::uint64_t total=0; std::string previous;
    while(std::getline(stream,line)) {
        const auto entry=fields(line);
        if(entry.size()!=4||entry[0]!="file"||entry[1].size()!=64||!std::all_of(entry[1].begin(),entry[1].end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');})) fail("invalid file record");
        if(!previous.empty()&&entry[3]<=previous) fail("file records must be unique and sorted"); previous=entry[3];
        const auto size=decimal(entry[2]); if(size>512ULL*1024*1024-total) fail("content exceeds 512 MiB"); total+=size;
        if(files.size()>=256) fail("more than 256 resource files"); files.emplace(entry[3],File{entry[1],size});
    }
    const auto root=manifest.parent_path().empty()?std::filesystem::path("."):manifest.parent_path();
    if(!files.contains(scene_name)) fail("scene missing from file records");
    // Every byte used to identify a session is verified against local files first.
    for(const auto& [name,expected]:files) if(hash_file(local_path(root,name),expected.size)!=expected.hash) fail("resource SHA256 mismatch: "+name);
    const auto description=describe_scene(root,scene_name);
    if(description.clips!=result.clips) fail("clip order/name differs from glTF");
    const auto& references=description.references;
    if(references.size()!=files.size()) fail("manifest does not exactly cover glTF resources");
    for(const auto& reference:references) if(!files.contains(reference)) fail("missing glTF dependency: "+reference);
    picosha2::hash256(document.begin(),document.end(),result.digest.begin(),result.digest.end());
    result.hex=picosha2::bytes_to_hex_string(result.digest.begin(),result.digest.end()); return result;
}
WorldIdentity identify_world(const std::filesystem::path& recipe,const std::array<std::filesystem::path,4>& scenes) {
    const auto absolute=std::filesystem::canonical(recipe),root=absolute.parent_path();
    const auto relative=[&](const std::filesystem::path& file) {
        const auto name=std::filesystem::relative(file,root).generic_u8string();
        const std::string text(name.begin(),name.end()); local_path(root,text);return text;
    };
    const std::string recipe_name=relative(absolute);
    std::set<std::string> references{recipe_name};
    WorldIdentity result;
    std::string document="MH4U-NATIVE-WORLD 1\nprofile\tdisk-floor-v1;protocol=2;initial-selection=0;tick-ms=50;hold-ms=250;axis=1000;turn=pi;skin=0.001;max-step=4096;contacts=8\nrecipe\t"+recipe_name+"\n";
    const auto append=[&](const std::string& line) {
        if(line.size()>1024*1024-document.size()) fail("world identity exceeds 1 MiB");
        document+=line;
    };
    const std::array<std::string,4> roles{"stage","actor","floor","walls"};
    for(std::size_t i=0;i<scenes.size();++i) {
        const auto name=relative(scenes[i]);
        SceneDescription description;
        if(i==1&&scenes[i].extension()==".mhnative") {
            auto loaded=native_actor::Actor::load(local_path(root,name));
            if(const auto* error=std::get_if<native_actor::Error>(&loaded))fail("native actor: "+error->message);
            const auto& actor=std::get<native_actor::Actor>(loaded);
            for(const auto& file:actor.source_files())description.references.insert(relative(file));
            for(const auto& clip:actor.presentation()->animations)description.clips.push_back(clip.name);
        } else description=describe_scene(root,name,true);
        references.insert(description.references.begin(),description.references.end());
        if(references.size()>256) fail("world exceeds 256 files");
        append("role\t"+roles[i]+"\t"+name+"\n");
        if(i==1) result.content.clips=description.clips;
    }
    append("clips\t"+std::to_string(result.content.clips.size())+"\n");
    for(std::size_t i=0;i<result.content.clips.size();++i) append("clip\t"+std::to_string(i)+"\t"+result.content.clips[i]+"\n");
    for(const auto& name:references) {
        const auto file=local_path(root,name);const auto size=std::filesystem::file_size(file);
        if(size>512ULL*1024*1024-result.byte_count) fail("world exceeds 512 MiB");
        if(name==recipe_name&&size>64*1024) fail("world recipe exceeds 64 KiB");
        result.byte_count+=size;
        append("file\t"+hash_file(file,size)+"\t"+std::to_string(size)+"\t"+name+"\n");
    }
    picosha2::hash256(document.begin(),document.end(),result.content.digest.begin(),result.content.digest.end());
    result.content.hex=picosha2::bytes_to_hex_string(result.content.digest.begin(),result.content.digest.end());
    result.document=std::move(document);result.file_count=references.size();return result;
}

}

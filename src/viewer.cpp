#include "assets.hpp"
#include "scene.hpp"
#include "action_scene.hpp"
#include "native_actor.hpp"
#include "player_preview.hpp"
#ifdef MH4U_HAS_MODEL_MOTION
#include "model_motion_identity.hpp"
#endif
#ifdef MH4U_HAS_MOTION_EVENTS
#include "motion_events_identity.hpp"
#endif
#include "texture_pack.hpp"
#include "localization.hpp"
#ifdef MH4U_HAS_NATIVE_CATALOG
#include "catalog_identity.hpp"
#endif
#include "catalog.hpp"
#include "world_config.hpp"
#include "locomotion.hpp"
#include "net/world_client.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using mh4u::Vec3;
using Mat4=std::array<float,16>;
constexpr float pi=3.14159265358979323846f;
[[noreturn]] void fail(const std::string& reason) { throw std::runtime_error(reason); }
void check(bool ok, const char* action) { if (!ok) fail(std::string(action)+": "+SDL_GetError()); }
std::filesystem::path path_from_utf8(std::string_view text) {
    std::u8string value; value.reserve(text.size());
    for(unsigned char c:text) value.push_back(static_cast<char8_t>(c));
    return std::filesystem::path(value);
}
std::string path_text(const std::filesystem::path& path) {
    const auto value=path.u8string(); return std::string(value.begin(),value.end());
}
Vec3 subtract(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
float dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vec3 normalized(Vec3 a) { const auto n=std::hypot(a.x,a.y,a.z); return {a.x/n,a.y/n,a.z/n}; }
Mat4 multiply(const Mat4& a,const Mat4& b) {
    Mat4 c{};
    for (int col=0;col<4;++col) for(int row=0;row<4;++row) for(int k=0;k<4;++k)
        c[col*4+row]+=a[k*4+row]*b[col*4+k];
    return c;
}
Mat4 perspective(float aspect,float near_plane,float far_plane) {
    const float f=1/std::tan(45*pi/360);
    Mat4 m{}; m[0]=f/aspect; m[5]=f; m[10]=(far_plane+near_plane)/(near_plane-far_plane);
    m[11]=-1; m[14]=2*far_plane*near_plane/(near_plane-far_plane); return m;
}
Mat4 look_at(Vec3 eye,Vec3 target) {
    const Vec3 forward=normalized(subtract(target,eye));
    const Vec3 side=normalized(cross(forward,{0,1,0}));
    const Vec3 up=cross(side,forward);
    return {side.x,up.x,-forward.x,0,side.y,up.y,-forward.y,0,side.z,up.z,-forward.z,0,
            -dot(side,eye),-dot(up,eye),dot(forward,eye),1};
}
struct Camera {
    float yaw=0.6f,pitch=0.8f,distance=3.3f;
    void reset() { yaw=0.6f; pitch=0.8f; distance=3.3f; }
    void constrain() { pitch=std::clamp(pitch,-1.5f,1.5f); distance=std::clamp(distance,1.05f,20.0f); }
    Mat4 matrix(const mh4u::Mesh& mesh,float aspect) const {
        const float d=distance*mesh.radius;
        const Vec3 eye{mesh.center.x+d*std::cos(pitch)*std::sin(yaw),mesh.center.y+d*std::sin(pitch),
                       mesh.center.z+d*std::cos(pitch)*std::cos(yaw)};
        return multiply(perspective(aspect,mesh.radius*0.01f,mesh.radius*50),look_at(eye,mesh.center));
    }
};

struct Options {
    std::filesystem::path model,texture,alternative,alpha,alternative_alpha,screenshot,catalog,texture_pack,text_catalog,world,default_catalog;
    std::filesystem::path motion_package,paired_root,events_package,player_preview;
    int player_ticks=-1,player_state=1;
    bool explicit_player_state=false;
    int motion_action=-1,motion_ticks=-1;
    bool motion_repeat=false;
    double world_step_x=0,world_step_z=0;
    int connect_port=0;
    std::string player_name="Cacador";
    bool explicit_player_name=false;
    std::string text_document;
    int text_fragment=0;
    bool translated=true;
    int texture_mode=-1;
    int frames=0,resource=0;
    bool explicit_resource=false;
    bool inspect_only=false,trace_input=false;
    int animation=-2;
    float time=0;
    bool fixed_time=false,paused=false,loop=true;
    std::vector<int> hidden_primitives;
};
int integer(std::string_view text) {
    int value{};
    const auto [p,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(error!=std::errc{} || p!=text.data()+text.size()) fail("Valor inteiro inválido: "+std::string(text));
    return value;
}
Options options(int argc,char** argv) {
    Options result;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg=="--inspect-only") { result.inspect_only=true; continue; }
        if(arg=="--trace-input") { result.trace_input=true; continue; }
        if(arg=="--paused") { result.paused=true; continue; }
        if(arg=="--no-loop") { result.loop=false; continue; }
        if(arg=="--motion-repeat") { if(result.motion_repeat)fail("--motion-repeat duplicado");result.motion_repeat=true;continue; }
        if(arg=="--help") {
            std::cout<<"mh4u-resource-lab --model mesh.obj|scene.gltf|scene.glb|actor.mhnative [--texture image.ppm --alpha alpha.pgm]\n"
                       "  [--texture-alternative image.ppm --alpha-alternative alpha.pgm]\n"
                       "  [--texture-pack manifest.tsv] (PNG por identidade, com fallback original)\n"
                       "  [--texture-mode original|alternative|none]\n"
                       "  [--text-catalog catalog.mhtxt --text-language pt-BR|en --text-document NAME --text-fragment N]\n"
                       "  [--animation N|-1 --time SECONDS --paused --no-loop]\n"
                       "  [--hide-primitive N ...] (inspeção da cena inicial)\n"
                       "  [--inspect-only] [--frames N --screenshot capture.ppm] [--trace-input]\n"
                       "  Ou: --catalog catalog.tsv [--resource N] no lugar de --model/texturas\n"
                       "  Ou: --world world.tsv (cena controlável de diagnóstico)\n"
                       "  [--world-step-x UNITS --world-step-z UNITS] (um passo inicial de diagnóstico)\n"
                       "  [--connect-port PORT --player-name NAME] (servidor local de movimento)\n"
                       "  Ou: --motion-package model-motion.bin --paired-root PATH --motion-action N [--events-package motion-events.bin]\n"
                       "  [--motion-repeat] [--motion-ticks N --frames N --screenshot capture.ppm]\n"
                       "  Ou: --player-preview constants.bin com --world ou --model actor.mhnative\n"
                       "  [--player-state 1|3 --player-ticks N] (controlador em inspeção; sem bootstrap)\n"
                       "  Inspeção de ação: Espaço pausa, ponto avança um tick, Home reinicia.\n"
                       "  Política diagnóstica: 1 frame de origem/tick, 60 ticks/s assumidos.\n"
                       "Viewer nativo de recursos; não implementa gameplay ou runtime de 3DS.\n";
            std::exit(0);
        }
        if(i+1>=argc) fail("Argumento sem valor: "+arg);
        const std::string value=argv[++i];
        if(arg=="--model") result.model=path_from_utf8(value);
        else if(arg=="--world") result.world=path_from_utf8(value);
        else if(arg=="--player-preview") { if(!result.player_preview.empty())fail("--player-preview duplicado");result.player_preview=path_from_utf8(value); }
        else if(arg=="--player-ticks") { if(result.player_ticks!=-1)fail("--player-ticks duplicado");result.player_ticks=integer(value);if(result.player_ticks<0||result.player_ticks>100000)fail("--player-ticks deve estar entre 0 e 100000"); }
        else if(arg=="--player-state") { if(result.explicit_player_state)fail("--player-state duplicado");result.explicit_player_state=true;result.player_state=integer(value);if(result.player_state!=1&&result.player_state!=3)fail("--player-state admite 1 ou 3 neste perfil"); }
        else if(arg=="--motion-package") { if(!result.motion_package.empty())fail("--motion-package duplicado");result.motion_package=path_from_utf8(value); }
        else if(arg=="--paired-root") { if(!result.paired_root.empty())fail("--paired-root duplicado");result.paired_root=path_from_utf8(value); }
        else if(arg=="--events-package") { if(!result.events_package.empty())fail("--events-package duplicado");result.events_package=path_from_utf8(value); }
        else if(arg=="--motion-action") { if(result.motion_action!=-1)fail("--motion-action duplicado");result.motion_action=integer(value);if(result.motion_action<0)fail("--motion-action exige índice não negativo"); }
        else if(arg=="--motion-ticks") { if(result.motion_ticks!=-1)fail("--motion-ticks duplicado");result.motion_ticks=integer(value);if(result.motion_ticks<0||result.motion_ticks>int(mh4u::ActionScene::max_capture_ticks))fail("--motion-ticks deve estar entre 0 e 100000"); }
        else if(arg=="--connect-port") { result.connect_port=integer(value);if(result.connect_port<1||result.connect_port>65535)fail("Porta deve estar entre 1 e 65535"); }
        else if(arg=="--player-name") { if(!mh4u::net::valid_name(value))fail("Nome de jogador inválido");result.player_name=value;result.explicit_player_name=true; }
        else if(arg=="--world-step-x"||arg=="--world-step-z") {
            double& target=arg=="--world-step-x"?result.world_step_x:result.world_step_z;
            const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),target);
            if(error!=std::errc{}||end!=value.data()+value.size()||!std::isfinite(target))fail("Passo de cena deve ser finito");
        }
        else if(arg=="--catalog") result.catalog=path_from_utf8(value);
        else if(arg=="--default-catalog") result.default_catalog=path_from_utf8(value);
        else if(arg=="--resource") { result.resource=integer(value); result.explicit_resource=true; if(result.resource<0||result.resource>=64) fail("--resource deve estar entre 0 e 63"); }
        else if(arg=="--texture") result.texture=path_from_utf8(value);
        else if(arg=="--texture-alternative") result.alternative=path_from_utf8(value);
        else if(arg=="--texture-pack") { if(!result.texture_pack.empty())fail("--texture-pack duplicado");result.texture_pack=path_from_utf8(value); }
        else if(arg=="--texture-mode") {
            if(value=="original")result.texture_mode=0;
            else if(value=="alternative")result.texture_mode=1;
            else if(value=="none")result.texture_mode=2;
            else fail("Modo de textura inválido");
        }
        else if(arg=="--text-catalog") result.text_catalog=path_from_utf8(value);
        else if(arg=="--text-document") result.text_document=value;
        else if(arg=="--text-fragment") { result.text_fragment=integer(value);if(result.text_fragment<0)fail("Fragmento de texto negativo"); }
        else if(arg=="--text-language") { if(value!="pt-BR"&&value!="en")fail("Idioma não suportado");result.translated=value=="pt-BR"; }
        else if(arg=="--alpha") result.alpha=path_from_utf8(value);
        else if(arg=="--alpha-alternative") result.alternative_alpha=path_from_utf8(value);
        else if(arg=="--screenshot") result.screenshot=path_from_utf8(value);
        else if(arg=="--animation") { result.animation=integer(value); if(result.animation<-1) fail("--animation exige índice ou -1 para pose de referência"); }
        else if(arg=="--hide-primitive") {
            const int index=integer(value);
            if(index<0)fail("--hide-primitive exige índice não negativo");
            if(std::find(result.hidden_primitives.begin(),result.hidden_primitives.end(),index)!=result.hidden_primitives.end())fail("--hide-primitive duplicado");
            result.hidden_primitives.push_back(index);
        }
        else if(arg=="--time") {
            const auto [p,error]=std::from_chars(value.data(),value.data()+value.size(),result.time);
            if(error!=std::errc{}||p!=value.data()+value.size()||!std::isfinite(result.time)||result.time<0) fail("--time exige segundos finitos não negativos");
            result.fixed_time=true;
        }
        else if(arg=="--frames") { result.frames=integer(value); if(result.frames<1 || result.frames>1000000) fail("--frames deve estar entre 1 e 1000000"); }
        else fail("Opção desconhecida: "+arg);
    }
    const bool motion=!result.motion_package.empty();
    if(!result.player_preview.empty()) {
        if(motion||!result.catalog.empty()||result.explicit_resource||result.connect_port)fail("Player em inspeção exige --world ou --model; rede e outros perfis não são combináveis");
        if(result.world.empty()&&result.model.extension()!=".mhnative")fail("Player em inspeção exige --world com ator nativo ou --model actor.mhnative");
        if(result.animation!=-2||result.fixed_time||!result.loop)fail("Estado e tempo pertencem ao Player; use --player-state e --player-ticks");
        if(result.frames&&!result.paused&&result.player_ticks<0)fail("Captura do Player exige --player-ticks ou --paused");
    } else if(result.player_ticks!=-1||result.explicit_player_state)fail("Opções do Player requerem --player-preview");
    if(motion) {
        if(result.paired_root.empty()||result.motion_action<0)fail("Inspeção de ação exige --motion-package, --paired-root e --motion-action");
        if(!result.world.empty()||result.connect_port)fail("Inspeção de ação ainda não suporta --world nem --connect-port");
        if(!result.model.empty()||!result.catalog.empty()||result.explicit_resource||!result.texture.empty()||!result.alternative.empty()||!result.alpha.empty()||!result.alternative_alpha.empty())fail("O pacote pareado é o único dono dos recursos da ação");
        if(result.animation!=-2||result.fixed_time||!result.loop)fail("Tempo e clip da ação pertencem ao Player; use --motion-ticks e --motion-repeat");
        if(result.frames&&!result.paused&&result.motion_ticks<0)fail("Captura de ação exige --motion-ticks ou --paused");
    } else if(!result.paired_root.empty()||!result.events_package.empty()||result.motion_action!=-1||result.motion_ticks!=-1||result.motion_repeat)fail("Opções de ação requerem --motion-package");
    if(!motion&&result.model.empty()&&result.catalog.empty()&&result.world.empty())result.catalog=result.default_catalog;
    if(!motion&&result.model.empty()&&result.catalog.empty()&&result.world.empty()) fail("Informe --model, --catalog ou --world com recursos extraídos do jogo");
    if(!result.world.empty()&&(!result.model.empty()||!result.catalog.empty()||!result.texture.empty()||!result.alternative.empty()||!result.alpha.empty()||!result.alternative_alpha.empty()))fail("--world substitui --model, --catalog e texturas OBJ");
    if(result.world.empty()&&(result.world_step_x!=0||result.world_step_z!=0))fail("Passo de cena requer --world");
    if(result.connect_port&&(result.world.empty()||result.world_step_x!=0||result.world_step_z!=0||result.inspect_only))fail("Conexão requer --world, sem passo inicial nem --inspect-only");
    if(result.explicit_player_name&&!result.connect_port)fail("--player-name requer --connect-port");
    if(!result.catalog.empty()&&(!result.model.empty()||!result.texture.empty()||!result.alternative.empty()||!result.alpha.empty()||!result.alternative_alpha.empty())) fail("--catalog substitui --model e as opções de textura");
    if(result.explicit_resource&&result.catalog.empty()) fail("--resource requer --catalog");
    if(!result.texture_pack.empty()&&!result.alternative.empty())fail("Escolha --texture-pack ou --texture-alternative");
    if(!result.alpha.empty() && result.texture.empty()) fail("--alpha requer --texture");
    if(!result.alternative_alpha.empty() && result.alternative.empty()) fail("--alpha-alternative requer --texture-alternative");
    if(!result.screenshot.empty() && result.frames==0) fail("--screenshot requer --frames para captura reproduzível");
    return result;
}
struct SDLSession {
    SDLSession() { check(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD),"Inicializar SDL"); }
    ~SDLSession() { SDL_Quit(); }
};
struct Window {
    SDL_Window* window{};
    SDL_GLContext context{};
    Window() {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
#ifdef __APPLE__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
#endif
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
        window=SDL_CreateWindow("MH4U Native · Laboratório de recursos",1280,800,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
        check(window!=nullptr,"Criar janela");
        context=SDL_GL_CreateContext(window);
        if(!context) { SDL_DestroyWindow(window); window=nullptr; check(false,"Criar contexto OpenGL"); }
        SDL_GL_SetSwapInterval(1);
    }
    ~Window() { if(context) SDL_GL_DestroyContext(context); if(window) SDL_DestroyWindow(window); }
};
struct Gui {
    bool sdl=false,gl=false;
    explicit Gui(const Window& w) {
        IMGUI_CHECKVERSION(); ImGui::CreateContext();
        try {
            auto& io=ImGui::GetIO(); io.IniFilename=nullptr;
            ImGui::StyleColorsDark();
            auto& style=ImGui::GetStyle(); style.WindowRounding=0; style.FrameRounding=4;
            style.WindowPadding={20,20}; style.ItemSpacing={10,10}; style.FramePadding={8,6};
            style.Colors[ImGuiCol_WindowBg]={0.075f,0.09f,0.105f,1};
            style.Colors[ImGuiCol_Button]={0.17f,0.27f,0.29f,1};
            style.Colors[ImGuiCol_ButtonHovered]={0.22f,0.38f,0.39f,1};
            style.Colors[ImGuiCol_CheckMark]={0.48f,0.78f,0.69f,1};
            style.Colors[ImGuiCol_SliderGrab]={0.48f,0.78f,0.69f,1};
            ImFontConfig config; config.SizePixels=17;
            io.Fonts->AddFontDefault(&config);
            sdl=ImGui_ImplSDL3_InitForOpenGL(w.window,w.context);
            if(!sdl) fail("Falha ao inicializar interface SDL");
            gl=ImGui_ImplOpenGL3_Init("#version 150");
            if(!gl) fail("Falha ao inicializar interface OpenGL");
        } catch(...) { if(gl) ImGui_ImplOpenGL3_Shutdown(); if(sdl) ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext(); throw; }
    }
    ~Gui() { if(gl) ImGui_ImplOpenGL3_Shutdown(); if(sdl) ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext(); }
};

// Explicit diagnostic only: button/focus state, never keyboard text. Bound output
// so a forgotten diagnostic window cannot grow a redirected log indefinitely.
struct InputTrace {
    unsigned remaining=0,follow_frames=0;
    Uint64 next_sample=0;
    explicit InputTrace(bool enabled):remaining(enabled?1024:0) {
        if(enabled) { std::cout.setf(std::ios::unitbuf); std::cout<<"INPUT_TRACE enabled max_records=1024 keyboard_text=off\n"; }
    }
    bool record() {
        if(!remaining)return false;
        if(--remaining==0)std::cout<<"INPUT_TRACE record_limit_reached\n";
        return true;
    }
    static SDL_WindowID focus_id(SDL_Window* window) { return window?SDL_GetWindowID(window):0; }
    void event(const SDL_Event& event,bool accepted,SDL_Window* window) {
        const bool button=event.type==SDL_EVENT_MOUSE_BUTTON_DOWN||event.type==SDL_EVENT_MOUSE_BUTTON_UP;
        const bool focus=event.type==SDL_EVENT_WINDOW_FOCUS_GAINED||event.type==SDL_EVENT_WINDOW_FOCUS_LOST;
        if((!button&&!focus)||!record())return;
        follow_frames=4;
        std::cout<<"INPUT_EVENT type="<<event.type<<" accepted="<<accepted<<" window="<<(button?event.button.windowID:event.window.windowID)
                 <<" expected_window="<<SDL_GetWindowID(window)<<" keyboard_focus="<<focus_id(SDL_GetKeyboardFocus())
                 <<" mouse_focus="<<focus_id(SDL_GetMouseFocus());
        if(button)std::cout<<" button="<<unsigned(event.button.button)<<" down="<<event.button.down<<" x="<<event.button.x<<" y="<<event.button.y;
        std::cout<<'\n';
    }
    void frame(bool playing,int animation) {
        const auto now=SDL_GetTicks();
        if(!remaining||(!follow_frames&&now<next_sample))return;
        if(follow_frames)--follow_frames;
        next_sample=now+2000;
        if(!record())return;
        const auto& io=ImGui::GetIO();
        std::cout<<"INPUT_FRAME frame="<<ImGui::GetFrameCount()<<" mouse="<<io.MousePos.x<<','<<io.MousePos.y
                 <<" down="<<io.MouseDown[0]<<" clicked="<<ImGui::IsMouseClicked(0)<<" released="<<ImGui::IsMouseReleased(0)
                 <<" capture="<<io.WantCaptureMouse<<" playing="<<playing<<" animation="<<animation<<'\n';
    }
    void playback(bool playing) { if(record())std::cout<<"INPUT_ACTION playback="<<playing<<'\n'; }
};

template<typename T> T load_gl(const char* name) {
    auto ptr=SDL_GL_GetProcAddress(name);
    if(!ptr) fail(std::string("OpenGL sem função necessária: ")+name);
    return reinterpret_cast<T>(ptr);
}
struct GLApi {
#define GL_PROC(type,name) type name=load_gl<type>("gl" #name)
    GL_PROC(PFNGLGENVERTEXARRAYSPROC,GenVertexArrays);
    GL_PROC(PFNGLBINDVERTEXARRAYPROC,BindVertexArray);
    GL_PROC(PFNGLDELETEVERTEXARRAYSPROC,DeleteVertexArrays);
    GL_PROC(PFNGLGENBUFFERSPROC,GenBuffers);
    GL_PROC(PFNGLBINDBUFFERPROC,BindBuffer);
    GL_PROC(PFNGLBUFFERDATAPROC,BufferData);
    GL_PROC(PFNGLDELETEBUFFERSPROC,DeleteBuffers);
    GL_PROC(PFNGLVERTEXATTRIBPOINTERPROC,VertexAttribPointer);
    GL_PROC(PFNGLENABLEVERTEXATTRIBARRAYPROC,EnableVertexAttribArray);
    GL_PROC(PFNGLCREATESHADERPROC,CreateShader);
    GL_PROC(PFNGLSHADERSOURCEPROC,ShaderSource);
    GL_PROC(PFNGLCOMPILESHADERPROC,CompileShader);
    GL_PROC(PFNGLGETSHADERIVPROC,GetShaderiv);
    GL_PROC(PFNGLGETSHADERINFOLOGPROC,GetShaderInfoLog);
    GL_PROC(PFNGLDELETESHADERPROC,DeleteShader);
    GL_PROC(PFNGLCREATEPROGRAMPROC,CreateProgram);
    GL_PROC(PFNGLATTACHSHADERPROC,AttachShader);
    GL_PROC(PFNGLLINKPROGRAMPROC,LinkProgram);
    GL_PROC(PFNGLGETPROGRAMIVPROC,GetProgramiv);
    GL_PROC(PFNGLGETPROGRAMINFOLOGPROC,GetProgramInfoLog);
    GL_PROC(PFNGLDELETEPROGRAMPROC,DeleteProgram);
    GL_PROC(PFNGLUSEPROGRAMPROC,UseProgram);
    GL_PROC(PFNGLBINDATTRIBLOCATIONPROC,BindAttribLocation);
    GL_PROC(PFNGLGETUNIFORMLOCATIONPROC,GetUniformLocation);
    GL_PROC(PFNGLUNIFORMMATRIX4FVPROC,UniformMatrix4fv);
    GL_PROC(PFNGLUNIFORM1IPROC,Uniform1i);
    GL_PROC(PFNGLUNIFORM1FPROC,Uniform1f);
    GL_PROC(PFNGLUNIFORM4FVPROC,Uniform4fv);
    GL_PROC(PFNGLACTIVETEXTUREPROC,ActiveTexture);
#undef GL_PROC
};
class Renderer {
public:
    Renderer() {
        const char* vertex=R"(#version 150
in vec3 aPosition; in vec3 aNormal; in vec2 aUV;
uniform mat4 uMVP; uniform mat4 uModel; uniform bool uFlipV;
out vec3 normal; out vec2 uv;
void main(){ gl_Position=uMVP*vec4(aPosition,1); normal=mat3(uModel)*aNormal; uv=vec2(aUV.x,uFlipV ? 1.0-aUV.y : aUV.y); }
)";
        const char* fragment=R"(#version 150
in vec3 normal; in vec2 uv;
uniform sampler2D uTexture; uniform bool uTextured; uniform bool uLighting;
uniform int uAlphaMode; uniform float uAlphaCutoff; uniform vec4 uColor;
out vec4 outColor;
void main(){
 vec3 n=normalize(normal); if(!gl_FrontFacing) n=-n;
 float light=uLighting ? 0.52+0.48*max(dot(n,normalize(vec3(0.6,0.8,0.9))),0.0) : 1.0;
 vec4 color=uColor*(uTextured ? texture(uTexture,uv) : vec4(1));
 if(uAlphaMode==0) color.a=1.0;
 else if(uAlphaMode==1) { if(color.a<uAlphaCutoff) discard; color.a=1.0; }
 else if(color.a<0.01) discard;
 outColor=vec4(color.rgb*light,color.a);
}
)";
        const GLuint vs=shader(GL_VERTEX_SHADER,vertex);
        GLuint fs{};
        try {
            fs=shader(GL_FRAGMENT_SHADER,fragment);
            program_=gl_.CreateProgram(); gl_.AttachShader(program_,vs); gl_.AttachShader(program_,fs);
            gl_.BindAttribLocation(program_,0,"aPosition"); gl_.BindAttribLocation(program_,1,"aNormal"); gl_.BindAttribLocation(program_,2,"aUV");
            gl_.LinkProgram(program_);
            GLint success=0; gl_.GetProgramiv(program_,GL_LINK_STATUS,&success);
            if(!success) { std::array<char,4096> log{}; gl_.GetProgramInfoLog(program_,log.size(),nullptr,log.data()); fail(std::string("Link de shader falhou: ")+log.data()); }
            gl_.GenVertexArrays(1,&vao_); gl_.GenBuffers(1,&vbo_);
            fallback_=texture({1,1,{255,255,255},{}});
        } catch(...) { gl_.DeleteShader(vs); if(fs) gl_.DeleteShader(fs); cleanup(); throw; }
        gl_.DeleteShader(vs); gl_.DeleteShader(fs);
        mvp_=gl_.GetUniformLocation(program_,"uMVP"); flip_=gl_.GetUniformLocation(program_,"uFlipV");
        model_=gl_.GetUniformLocation(program_,"uModel");
        textured_=gl_.GetUniformLocation(program_,"uTextured"); lighting_=gl_.GetUniformLocation(program_,"uLighting");
        sampler_=gl_.GetUniformLocation(program_,"uTexture");
        alpha_=gl_.GetUniformLocation(program_,"uAlphaMode");
        cutoff_=gl_.GetUniformLocation(program_,"uAlphaCutoff"); color_=gl_.GetUniformLocation(program_,"uColor");
    }
    ~Renderer() { cleanup(); }
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    void upload(const mh4u::Mesh& mesh,bool dynamic=false) {
        gl_.BindVertexArray(vao_); gl_.BindBuffer(GL_ARRAY_BUFFER,vbo_);
        gl_.BufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(mesh.triangles.size()*sizeof(mh4u::Vertex)),mesh.triangles.data(),dynamic?GL_DYNAMIC_DRAW:GL_STATIC_DRAW);
        const std::array<std::size_t,3> offsets{offsetof(mh4u::Vertex,position),offsetof(mh4u::Vertex,normal),offsetof(mh4u::Vertex,uv)};
        for(GLuint i=0;i<3;++i) { gl_.EnableVertexAttribArray(i); gl_.VertexAttribPointer(i,i==2?2:3,GL_FLOAT,GL_FALSE,sizeof(mh4u::Vertex),reinterpret_cast<const void*>(offsets[i])); }
        count_=static_cast<GLsizei>(mesh.triangles.size()); gl_.BindVertexArray(0);
        if(glGetError()!=GL_NO_ERROR) fail("OpenGL rejeitou os buffers da malha");
    }
    GLuint texture(const mh4u::Image& image) {
        GLint max_size=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max_size);
        if(image.width>max_size || image.height>max_size) fail("Textura excede a dimensão suportada pela GPU");
        GLuint id=0; glGenTextures(1,&id);
        try { textures_.push_back(id); }
        catch(...) { glDeleteTextures(1,&id); throw; }
        glBindTexture(GL_TEXTURE_2D,id);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        if(image.alpha.empty()) glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,image.width,image.height,0,GL_RGB,GL_UNSIGNED_BYTE,image.rgb.data());
        else {
            if(image.alpha.size()!=image.rgb.size()/3) fail("Tamanho de alpha inconsistente");
            std::vector<std::uint8_t> rgba(image.alpha.size()*4);
            for(std::size_t i=0;i<image.alpha.size();++i) {
                for(std::size_t component=0;component<3;++component) rgba[i*4+component]=image.rgb[i*3+component];
                rgba[i*4+3]=image.alpha[i];
            }
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        }
        if(glGetError()!=GL_NO_ERROR) fail("OpenGL rejeitou a textura");
        return id;
    }
    void sampler(GLuint texture,const mh4u::Material& material) {
        const auto wrap=[](mh4u::WrapMode mode) { return mode==mh4u::WrapMode::clamp?GL_CLAMP_TO_EDGE:(mode==mh4u::WrapMode::mirror?GL_MIRRORED_REPEAT:GL_REPEAT); };
        glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap(material.wrap_s)); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap(material.wrap_t));
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,material.min_linear?GL_LINEAR:GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,material.mag_linear?GL_LINEAR:GL_NEAREST);
    }
    void draw(const Mat4& mvp,GLuint texture,bool flip,bool lighting,bool wireframe,int alpha_mode,
              std::size_t first=0,std::size_t count=0,std::array<float,4> color={1,1,1,1},float cutoff=0.5f,
              const Mat4& model=mh4u::identity_matrix()) {
        glEnable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glPolygonMode(GL_FRONT_AND_BACK,wireframe ? GL_LINE : GL_FILL);
        gl_.UseProgram(program_); gl_.UniformMatrix4fv(mvp_,1,GL_FALSE,mvp.data());
        gl_.UniformMatrix4fv(model_,1,GL_FALSE,model.data());
        gl_.Uniform1i(flip_,flip); gl_.Uniform1i(textured_,texture!=0); gl_.Uniform1i(lighting_,lighting); gl_.Uniform1i(sampler_,0);
        gl_.Uniform1i(alpha_,alpha_mode); gl_.Uniform1f(cutoff_,cutoff); gl_.Uniform4fv(color_,1,color.data());
        gl_.ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture?texture:fallback_); gl_.BindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES,static_cast<GLint>(first),count?static_cast<GLsizei>(count):count_);
        gl_.BindVertexArray(0); gl_.UseProgram(0); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    }
private:
    GLuint shader(GLenum kind,const char* source) {
        const GLuint shader=gl_.CreateShader(kind); gl_.ShaderSource(shader,1,&source,nullptr); gl_.CompileShader(shader);
        GLint success=0; gl_.GetShaderiv(shader,GL_COMPILE_STATUS,&success);
        if(!success) { std::array<char,4096> log{}; gl_.GetShaderInfoLog(shader,log.size(),nullptr,log.data()); gl_.DeleteShader(shader); fail(std::string("Compilação de shader falhou: ")+log.data()); }
        return shader;
    }
    void cleanup() {
        if(!textures_.empty()) glDeleteTextures(static_cast<GLsizei>(textures_.size()),textures_.data());
        if(vbo_) gl_.DeleteBuffers(1,&vbo_); if(vao_) gl_.DeleteVertexArrays(1,&vao_); if(program_) gl_.DeleteProgram(program_);
    }
    GLApi gl_;
    GLuint program_{},vao_{},vbo_{},fallback_{};
    GLsizei count_{};
    GLint mvp_{},model_{},flip_{},textured_{},lighting_{},sampler_{},alpha_{},cutoff_{},color_{};
    std::vector<GLuint> textures_;
};

struct Pad {
    SDL_Gamepad* handle{};
    void refresh() {
        if(handle && !SDL_GamepadConnected(handle)) { SDL_CloseGamepad(handle); handle=nullptr; }
        if(handle) return;
        int count=0; auto ids=SDL_GetGamepads(&count);
        for(int i=0;i<count && !handle;++i) handle=SDL_OpenGamepad(ids[i]);
        SDL_free(ids);
    }
    ~Pad() { if(handle) SDL_CloseGamepad(handle); }
};
struct Controls {
    int axis_x=2,axis_y=3,reset=0;
    int move_x=0,move_y=1;
    float deadzone=0.16f,speed=1.8f;
    bool invert_x=false,invert_y=false;
};
constexpr std::array<SDL_GamepadButton,4> reset_buttons{SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_WEST,SDL_GAMEPAD_BUTTON_NORTH};
float axis(SDL_Gamepad* pad,int selected,float deadzone) {
    const auto value=SDL_GetGamepadAxis(pad,static_cast<SDL_GamepadAxis>(selected));
    const float normalized=value<0 ? value/32768.0f : value/32767.0f;
    const float magnitude=std::abs(normalized);
    return magnitude<=deadzone ? 0 : std::copysign((magnitude-deadzone)/(1-deadzone),normalized);
}
std::filesystem::path controls_path() {
    char* path=SDL_GetPrefPath("MH4UNative","ResourceLab");
    if(!path) fail(std::string("Pasta de preferências indisponível: ")+SDL_GetError());
    const auto result=path_from_utf8(path)/"controls.ini"; SDL_free(path); return result;
}
using Settings=std::map<std::string,std::string>;
Settings settings_read(const std::filesystem::path& path) {
    Settings entries;
    std::ifstream input(path);
    if(!input) {
        if(std::filesystem::exists(path)) fail("Não foi possível ler controls.ini");
        return entries;
    }
    std::string line;
    while(std::getline(input,line)) {
        if(line.size()>4096 || entries.size()>128) fail("Arquivo de controles excede o limite de configuração");
        if(line.empty() || line.starts_with('#')) continue;
        const auto split=line.find('=');
        if(split==std::string::npos) fail("Linha inválida em controls.ini");
        if(!entries.emplace(line.substr(0,split),line.substr(split+1)).second) fail("Chave duplicada em controls.ini");
    }
    return entries;
}
float setting_float(const std::string& text,float low,float high) {
    float value{}; auto [p,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(error!=std::errc{} || p!=text.data()+text.size() || !std::isfinite(value) || value<low || value>high) fail("Valor inválido na configuração de controles");
    return value;
}
Controls load_controls(const Settings& entries) {
    Controls c;
    for(const auto& [key,value]:entries) {
        if(key=="axis_x") c.axis_x=integer(value);
        else if(key=="axis_y") c.axis_y=integer(value);
        else if(key=="move_x") c.move_x=integer(value);
        else if(key=="move_y") c.move_y=integer(value);
        else if(key=="reset") c.reset=integer(value);
        else if(key=="deadzone") c.deadzone=setting_float(value,0,0.5f);
        else if(key=="speed") c.speed=setting_float(value,0.2f,5);
        else if(key=="invert_x" || key=="invert_y") {
            const int v=integer(value); if(v!=0 && v!=1) fail("Inversão de eixo inválida");
            (key=="invert_x"?c.invert_x:c.invert_y)=v!=0;
        }
    }
    if(c.axis_x<0 || c.axis_x>3 || c.axis_y<0 || c.axis_y>3 || c.move_x<0||c.move_x>3||c.move_y<0||c.move_y>3||c.reset<0 || c.reset>3) fail("Mapeamento de controle fora do intervalo");
    return c;
}
void save_controls(const std::filesystem::path& path,Settings entries,const Controls& c) {
    entries["axis_x"]=std::to_string(c.axis_x); entries["axis_y"]=std::to_string(c.axis_y); entries["reset"]=std::to_string(c.reset);
    entries["move_x"]=std::to_string(c.move_x);entries["move_y"]=std::to_string(c.move_y);
    entries["deadzone"]=std::to_string(c.deadzone); entries["speed"]=std::to_string(c.speed);
    entries["invert_x"]=c.invert_x?"1":"0"; entries["invert_y"]=c.invert_y?"1":"0";
    std::ostringstream serialized;
    for(const auto& [key,value]:entries) serialized<<key<<'='<<value<<'\n';
    if(std::filesystem::exists(path)) {
        std::ifstream input(path); std::ostringstream previous; previous<<input.rdbuf();
        if(previous.str()==serialized.str()) return;
    }
    auto temporary=path; temporary += ".tmp";
    {
        std::ofstream output(temporary,std::ios::trunc); if(!output) fail("Não foi possível escrever as preferências");
        output<<serialized.str();
        if(!output) fail("Falha ao salvar preferências");
    }
    if(std::filesystem::exists(path)) {
        std::filesystem::path backup=path; backup += ".bak";
        for(int i=1;std::filesystem::exists(backup);++i) { backup=path; backup += ".bak."+std::to_string(i); }
        std::filesystem::rename(path,backup);
        try { std::filesystem::rename(temporary,path); }
        catch(...) { std::filesystem::rename(backup,path); throw; }
    } else std::filesystem::rename(temporary,path);
}
void screenshot(const std::filesystem::path& path,int width,int height) {
    if(width<=0 || height<=0 || std::uint64_t(width)*height>64U*1024U*1024U/3) fail("Captura excede o limite de 64 MiB");
    mh4u::Image image{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*3),{}};
    glPixelStorei(GL_PACK_ALIGNMENT,1); glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,image.rgb.data());
    if(glGetError()!=GL_NO_ERROR) fail("OpenGL não concluiu a leitura da captura");
    const auto stride=std::size_t(width)*3;
    for(int row=0;row<height/2;++row) for(std::size_t column=0;column<stride;++column)
        std::swap(image.rgb[std::size_t(row)*stride+column],image.rgb[std::size_t(height-1-row)*stride+column]);
    mh4u::write_ppm(path,image);
}

mh4u::player_transition::Constants player_constants(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input||input.tellg()!=88)fail("Constantes do Player: esperado pacote de 88 bytes");
    std::array<std::uint8_t,88> bytes{};input.seekg(0);
    if(!input.read(reinterpret_cast<char*>(bytes.data()),bytes.size())||input.peek()!=std::char_traits<char>::eof())fail("Constantes do Player: leitura incompleta");
    auto decoded=mh4u::player_transition::Constants::decode(bytes,{
        "1cf3b6e6873f77008eb072e18e27579e6bec375622c7d4780413f06f1cee3d5f",
        "d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"});
    if(const auto* error=std::get_if<mh4u::player_transition::Error>(&decoded))fail("Constantes do Player: "+std::string(error->message));
    return std::get<mh4u::player_transition::Constants>(decoded);
}

struct LoadedResource {
    mh4u::ResourceSpec spec;
    std::shared_ptr<const mh4u::Scene> scene;
    std::unique_ptr<mh4u::native_actor::Actor> native;
    std::unique_ptr<mh4u::native_actor::PlayerPreview> player;
    int player_requested_state=-1;
    std::size_t native_joints=0,native_parts=0;
    float native_rate=0;
    float native_frame=0;
    std::size_t native_samples=0,native_palettes=0;
    std::uint64_t native_tick=0;std::array<std::uint32_t,4> native_events{};bool native_ended=false;
    mh4u::SceneFrame frame;
    mh4u::Mesh obj,bounds;
    std::unique_ptr<mh4u::Image> original,alternative;
    std::unique_ptr<Renderer> renderer;
    GLuint original_id=0,alternative_id=0;
    std::vector<GLuint> material_textures;
    std::vector<GLuint> pack_textures;
    const mh4u::TexturePack* pack=nullptr;
    std::size_t pack_matches=0;
    std::vector<bool> visible_primitives;
    int initial_animation=-1;
    const mh4u::Mesh& mesh() const { return scene?frame.mesh:obj; }
    void accept(mh4u::native_actor::Frame result) {
        native_frame=result.source_frame;native_samples=result.samples;native_palettes=result.palettes;
        native_tick=result.tick;native_events=result.event_masks;native_ended=result.ended;
        frame=std::move(result.scene);
    }
    void accept(mh4u::native_actor::PlayerResult<mh4u::native_actor::PlayerOutput> result) {
        if(const auto* error=std::get_if<mh4u::native_actor::PlayerError>(&result))fail("Player: "+error->message);
        accept(std::move(std::get<mh4u::native_actor::PlayerOutput>(result).frame));
    }
    void request_player(int state) { accept(player->request(static_cast<std::uint32_t>(state)));player_requested_state=state; }
    void reset_player() { accept(player->reset());player_requested_state=-1; }
    void enable_player(const std::filesystem::path& constants,int state,int ticks) {
        if(!native)fail("Player em inspeção exige ator MOD/LMT");
        auto candidate=mh4u::native_actor::PlayerPreview::from_actor(*native,player_constants(constants));
        if(const auto* error=std::get_if<mh4u::native_actor::PlayerError>(&candidate))fail("Player: "+error->message);
        player=std::make_unique<mh4u::native_actor::PlayerPreview>(std::get<mh4u::native_actor::PlayerPreview>(std::move(candidate)));
        native.reset(); // Player retains the shared immutable resource owner, not a second playback state.
        accept(player->capture());request_player(state);
        for(int i=0;i<ticks;++i)accept(player->step());
        bounds.center=mesh().center;bounds.radius=mesh().radius;
    }
    void evaluate(int animation,float seconds,bool loop,bool change=false,bool seek=false) {
        if(player)fail("Player não aceita seek de clip independente");
        if(native) {
            auto evaluated=change?native->change_action(animation,loop):(seek?native->seek(animation,seconds,loop):native->evaluate(animation,seconds,loop));
            if(auto* e=std::get_if<mh4u::native_actor::Error>(&evaluated))fail("Animação nativa: "+e->message);
            accept(std::get<mh4u::native_actor::Frame>(std::move(evaluated)));
        } else mh4u::evaluate_scene(*scene,animation,seconds,loop,frame);
    }
    LoadedResource(const mh4u::ResourceSpec& source,int animation,float seconds,bool fixed_time,bool loop,const mh4u::TexturePack* texture_pack=nullptr):spec(source),pack(texture_pack) {
        for(const auto* path:{&spec.model,&spec.texture,&spec.alpha,&spec.alternative,&spec.alternative_alpha})
            if(!path->empty()&&!std::filesystem::is_regular_file(*path))fail("Arquivo de recurso ausente: "+path_text(*path));
        const auto extension=spec.model.extension().string();
        if(extension==".gltf"||extension==".glb"||extension==".mhnative") {
            if(extension==".mhnative") {
                auto loaded=mh4u::native_actor::Actor::load(spec.model);
                if(auto* e=std::get_if<mh4u::native_actor::Error>(&loaded))fail("Ator nativo: "+e->message);
                native=std::make_unique<mh4u::native_actor::Actor>(std::get<mh4u::native_actor::Actor>(std::move(loaded)));
                scene=native->presentation();
                native_joints=native->joint_count();native_parts=native->part_count();native_rate=native->inspection_rate();
            } else scene=std::make_shared<mh4u::Scene>(mh4u::load_gltf(spec.model));
            if(pack)for(const auto& image:scene->images)if(pack->replacements.contains(image.encoded_sha256))++pack_matches;
            visible_primitives.assign(scene->primitives.size(),true);
            initial_animation=animation==-2?(scene->animations.empty()?-1:0):animation;
            evaluate(initial_animation,seconds,loop);
        } else {
            if(animation!=-2||fixed_time)fail("Opções de animação exigem uma cena animada");
            obj=mh4u::load_obj(spec.model);
        }
        bounds.center=mesh().center;bounds.radius=mesh().radius;
        if(!spec.texture.empty())original=std::make_unique<mh4u::Image>(mh4u::load_ppm(spec.texture));
        if(!spec.alternative.empty())alternative=std::make_unique<mh4u::Image>(mh4u::load_ppm(spec.alternative));
        if(!spec.alpha.empty())mh4u::apply_pgm_alpha(*original,spec.alpha);
        if(!spec.alternative_alpha.empty())mh4u::apply_pgm_alpha(*alternative,spec.alternative_alpha);
    }
    void upload() {
        renderer=std::make_unique<Renderer>();renderer->upload(mesh());
        original_id=original?renderer->texture(*original):0;
        alternative_id=alternative?renderer->texture(*alternative):0;
        if(scene)for(const auto& material:scene->materials) {
            GLuint id=0;
            if(material.image>=0){id=renderer->texture(scene->images[material.image].pixels);renderer->sampler(id,material);}
            material_textures.push_back(id);
            GLuint alternative_id=id;
            if(pack&&material.image>=0) {
                const auto replacement=pack->replacements.find(scene->images[material.image].encoded_sha256);
                if(replacement!=pack->replacements.end()) {
                    alternative_id=renderer->texture(replacement->second.pixels);
                    renderer->sampler(alternative_id,material);
                }
            }
            pack_textures.push_back(alternative_id);
        }
    }
    void report(float seconds) const {
        if(pack)std::cout<<"TEXTURE_PACK "<<pack->name<<" MATCHED_IMAGES "<<pack_matches<<" TOTAL_IMAGES "<<(scene?scene->images.size():0)<<'\n';
        if(scene) {
            if(player)std::cout<<"PLAYER_PROFILE diagnostic-player-driver-v1\nPLAYER_POLICY source_units_per_tick=1 assumed_ticks_per_second=60 bootstrap=unknown\nPLAYER_REQUEST_STATE "<<player_requested_state
                <<"\nPLAYER_TICKS "<<player->player().completed_steps()<<"\nPLAYER_FRAME "<<native_frame<<"\nPLAYER_SAMPLES "<<native_samples<<"\nPLAYER_PALETTES "<<native_palettes
                <<"\nPLAYER_JOINTS "<<native_joints<<"\nPLAYER_PARTS "<<native_parts<<'\n';
            if(native)std::cout<<"NATIVE_PROFILE mod-lmt-transition-v3\nNATIVE_JOINTS "<<native->joint_count()<<"\nNATIVE_PARTS "<<native->part_count()
                <<"\nNATIVE_RATE "<<native->inspection_rate()<<"\nNATIVE_FRAME "<<native_frame<<"\nNATIVE_TICK "<<native_tick<<"\nNATIVE_ENDED "<<native_ended<<"\nNATIVE_SAMPLES "<<native_samples<<"\nNATIVE_PALETTES "<<native_palettes<<'\n';
            std::cout<<"NODES "<<scene->nodes.size()<<"\nSKINS "<<scene->skins.size()<<"\nPRIMITIVES "<<scene->primitives.size()
                     <<"\nANIMATIONS "<<scene->animations.size()<<"\nANIMATION "<<initial_animation<<"\nTIME "<<seconds<<'\n';
            for(std::size_t i=0;i<scene->animations.size();++i)
                std::cout<<"CLIP "<<i<<' '<<scene->animations[i].name<<" SECONDS "<<scene->animations[i].duration<<'\n';
        }
        const auto& geometry=mesh();
        std::cout<<"MODEL "<<path_text(spec.model)<<"\nPOSITIONS "<<geometry.source_positions<<"\nTRIANGLES "<<geometry.triangles.size()/3
                 <<"\nBOUNDS "<<geometry.minimum.x<<' '<<geometry.minimum.y<<' '<<geometry.minimum.z<<" / "<<geometry.maximum.x<<' '<<geometry.maximum.y<<' '<<geometry.maximum.z<<'\n';
        if(original)std::cout<<"TEXTURE "<<original->width<<'x'<<original->height<<'\n';
    }
    void draw(const Mat4& view_projection,const Mat4& model,int mode,bool flip,bool lighting,bool wireframe,bool use_alpha) {
        const auto mvp=mh4u::matrix_product(view_projection,model);
        const GLuint texture=mode==0?original_id:(mode==1?alternative_id:0);
        if(scene) {
            for(const auto& range:frame.draws) {
                if(!visible_primitives[range.primitive])continue;
                const mh4u::Material empty;
                const auto& material=range.material>=0?scene->materials[range.material]:empty;
                const GLuint selected=mode==0?(original_id?original_id:(range.material>=0?material_textures[range.material]:0)):
                    (mode==1&&pack?(range.material>=0?pack_textures[range.material]:0):texture);
                const auto color=mode==2?std::array<float,4>{0.73f,0.76f,0.75f,1}:material.color;
                renderer->draw(mvp,selected,flip,lighting,wireframe,material.alpha==mh4u::AlphaMode::mask?1:0,
                    range.first,range.count,color,material.alpha_cutoff,model);
            }
        } else renderer->draw(mvp,texture,flip,lighting,wireframe,use_alpha?2:0,0,0,
            texture?std::array<float,4>{1,1,1,1}:std::array<float,4>{0.73f,0.76f,0.75f,1},0.5f,model);
    }
};

struct DiagnosticWorld {
    mh4u::WorldConfig config;
    std::optional<mh4u::net::WorldIdentity> network_identity;
    mh4u::locomotion::World geometry;
    mh4u::locomotion::State initial,state;
    mh4u::locomotion::StepResult last;
    std::size_t steps=0,blocked_steps=0;
    explicit DiagnosticWorld(const std::filesystem::path& path,bool network):config(mh4u::load_world_config(path)),
        network_identity(network?std::optional(mh4u::net::identify_world(path,{config.stage,config.actor,config.floor,config.walls})):std::nullopt),
        geometry(mh4u::collision::load_static_gltf(config.floor),mh4u::collision::load_static_gltf(config.walls),
                 {config.radius,config.height,config.min_normal_y,0}),
        initial(geometry.create_state(config.spawn.x,config.spawn.z,config.heading)),state(initial) {
        if(std::abs(initial.position.y-config.spawn.y)>0.01)fail("Ponto diagnóstico não corresponde ao piso da receita");
        last.state=state;
    }
    void step(double dx,double dz,double yaw_delta=0) {
        auto candidate=geometry.step(state,{dx,dz,yaw_delta});
        state=candidate.state;last=candidate;++steps;if(last.blocked)++blocked_steps;
    }
    Mat4 actor_transform(const mh4u::locomotion::State& actor) const {
        mh4u::Transform transform;
        transform.translation={static_cast<float>(actor.position.x),static_cast<float>(actor.position.y+config.actor_ground_offset),static_cast<float>(actor.position.z)};
        if(!std::isfinite(transform.translation.x)||!std::isfinite(transform.translation.y)||!std::isfinite(transform.translation.z))fail("Posição excede a precisão representável pelo renderer");
        transform.rotation={0,static_cast<float>(std::sin(actor.yaw/2)),0,static_cast<float>(std::cos(actor.yaw/2))};
        return mh4u::transform_matrix(transform);
    }
    Mat4 actor_transform() const { return actor_transform(state); }
    void report() const {
        const auto& stats=geometry.stats();
        const auto previous_precision=std::cout.precision();std::cout<<std::setprecision(17);
        std::cout<<"WORLD_PROFILE diagnostic-grounded-disk-v1\nWORLD_POSITION "<<state.position.x<<' '<<state.position.y<<' '<<state.position.z
                 <<"\nWORLD_YAW "<<state.yaw<<"\nWORLD_STEPS "<<steps<<"\nWORLD_BLOCKED_STEPS "<<blocked_steps
                 <<"\nWORLD_GEOMETRY floor="<<stats.floor_triangles<<" walls="<<stats.wall_triangles<<" boundaries="<<stats.floor_boundaries<<" segments="<<stats.wall_segments<<'\n';
        std::cout.precision(previous_precision);
    }
};

void report_motion(const mh4u::ActionScene& motion,const mh4u::SceneFrame& scene) {
    const auto previous=std::cout.precision();std::cout<<std::setprecision(9);
    const auto& frame=motion.current();const auto& p=frame.state.pre;const auto& c=p.clock;
    std::cout<<"MOTION_STATE\t"<<motion.ticks()<<'\t'<<frame.joint_source_frame<<'\t'<<c.frame<<'\t'<<c.internal_flags<<'\t'<<c.previous_flags<<'\t'<<frame.loop_iterations;
    const auto values=[](const auto& data){for(auto value:data)std::cout<<'\t'<<value;};
    values(p.model.position);values(p.model.rotation);values(c.previous_translation);values(c.previous_rotation);
    values(frame.state.prediction_delta.translation);values(frame.state.prediction_delta.rotation);std::cout<<'\n';
    if(motion.events_bound()) {std::cout<<"MOTION_EVENTS\t"<<motion.ticks();values(c.event_masks);values(c.previous_event_masks);std::cout<<'\n';}
    std::cout<<"MOTION_MODEL_MATRIX";values(motion.model_matrix());std::cout<<'\n';
    const auto bounds=motion.transformed_bounds(scene);
    std::cout<<"MOTION_RENDER_BOUNDS\t"<<bounds.minimum.x<<'\t'<<bounds.minimum.y<<'\t'<<bounds.minimum.z<<'\t'<<bounds.maximum.x<<'\t'<<bounds.maximum.y<<'\t'<<bounds.maximum.z<<'\n';
    std::cout.precision(previous);
}

int run(const Options& args) {
    std::unique_ptr<mh4u::ActionScene> motion;
    if(!args.motion_package.empty()) {
#ifdef MH4U_HAS_MODEL_MOTION
        std::optional<mh4u::ActionEventSource> events;
        if(!args.events_package.empty()) {
#ifdef MH4U_HAS_MOTION_EVENTS
            events=mh4u::ActionEventSource{args.events_package,mh4u::motion_events::generated::expected_package()};
#else
            fail("Recompile após gerar a identidade do pacote de eventos local");
#endif
        }
        motion=std::make_unique<mh4u::ActionScene>(args.motion_package,args.paired_root,mh4u::model_motion::generated::expected_package(),std::size_t(args.motion_action),args.motion_repeat,events);
        for(int i=0;i<args.motion_ticks;++i)motion->tick();
        const auto& action=motion->action();
        std::cout<<"MOTION_PROFILE diagnostic-paired-action-v1\nMOTION_POLICY source_frames_per_tick=1 assumed_ticks_per_second=60 repeat="<<motion->repeat()
                 <<"\nMOTION_ACTION "<<action.identity.entry_id<<' '<<action.identity.slot<<' '<<action.gltf_clip_index
                 <<"\nMOTION_COORDINATES source-model-XYZ identity-initial-placement raw-Q no-original-world-placement\n";
#else
        fail("Recompile após gerar a identidade do pacote de movimento local");
#endif
    }
    auto world=args.world.empty()?nullptr:std::make_unique<DiagnosticWorld>(args.world,args.connect_port!=0);
    const auto texture_pack=args.texture_pack.empty()?nullptr:std::make_unique<mh4u::TexturePack>(mh4u::load_texture_pack(args.texture_pack));
    std::optional<mh4u::localization::Catalog> texts;
    int text_document=0,text_fragment=args.text_fragment;
    bool translated=args.translated;
    if(!args.text_catalog.empty()) {
#ifdef MH4U_HAS_NATIVE_CATALOG
        auto loaded=mh4u::localization::Catalog::load(args.text_catalog,mh4u::localization::generated::expected_catalog());
        if(const auto* error=std::get_if<mh4u::localization::Error>(&loaded))fail("Catálogo de textos: "+error->message);
        texts=std::move(std::get<mh4u::localization::Catalog>(loaded));
        if(!args.text_document.empty()) {
            const auto& docs=texts->identities();
            const auto found=std::find_if(docs.begin(),docs.end(),[&](const auto& doc){return doc.document==args.text_document;});
            if(found==docs.end())fail("Documento de texto ausente");
            text_document=static_cast<int>(found-docs.begin());
        }
        const auto& identity=texts->identities()[text_document];
        if(static_cast<std::uint32_t>(text_fragment)>=identity.fragment_count)fail("Fragmento de texto fora do documento");
        const auto selected=texts->lookup(identity,static_cast<std::uint32_t>(text_fragment),translated?mh4u::localization::Language::translated:mh4u::localization::Language::english);
        if(const auto* error=std::get_if<mh4u::localization::Error>(&selected))fail(error->message);
        const auto& view=std::get<mh4u::localization::TextView>(selected);
        std::cout<<"TEXT_CATALOG fragments="<<texts->fragment_count()<<" translated="<<texts->translated_count()
                 <<"\nTEXT_SELECTED document="<<identity.document<<" fragment="<<text_fragment<<" locale="<<view.locale<<"\n";
#else
        fail("Recompile após gerar as identidades do catálogo de textos local");
#endif
    } else if(!args.text_document.empty()||args.text_fragment!=0||!args.translated)fail("Seleção de texto requer --text-catalog");
    std::vector<mh4u::ResourceSpec> catalog;
    if(!args.catalog.empty())catalog=mh4u::load_catalog(args.catalog);
    else if(motion)catalog.push_back({"Ação pareada em inspeção",motion->model_path(),{},{},{},{}});
    else if(world)catalog.push_back({"Caçador da cena diagnóstica",world->config.actor,{},{},{},{}});
    else catalog.push_back({path_text(args.model.filename()),args.model,args.texture,args.alpha,args.alternative,args.alternative_alpha});
    if(std::size_t(args.resource)>=catalog.size())fail("--resource fora do catálogo");
    int selected_resource=args.resource,pending_resource=-1;
    auto initial=std::make_unique<LoadedResource>(catalog[selected_resource],motion?int(motion->action().gltf_clip_index):args.animation,
        motion?motion->current().joint_source_frame:args.time,args.fixed_time,motion?false:args.loop,texture_pack.get());
    if(!args.player_preview.empty())initial->enable_player(args.player_preview,args.player_state,args.player_ticks);
    if(motion) {
        motion->verify_loaded_scene(*initial->scene);
        motion->evaluate(*initial->scene,initial->frame);
        initial->bounds=motion->transformed_bounds(initial->frame);
    }
    std::unique_ptr<LoadedResource> stage;
    if(world) {
        if(!initial->scene)fail("Ator da cena exige glTF ou receita nativa MOD/LMT");
        stage=std::make_unique<LoadedResource>(mh4u::ResourceSpec{"Vila",world->config.stage,{},{},{},{}},-1,0,false,false,texture_pack.get());
        if(!stage->scene||!stage->scene->skins.empty()||!stage->scene->animations.empty())fail("Cenário diagnóstico exige cena estática sem skins");
        if(args.world_step_x!=0||args.world_step_z!=0)world->step(args.world_step_x,args.world_step_z);
        (void)world->actor_transform();world->report();
    }
    if(args.texture_mode==1&&!initial->alternative&&!(texture_pack&&initial->scene))fail("Modo alternativo requer textura ou pack para cena glTF");
    initial->report(motion?motion->current().joint_source_frame:args.time);
    if(motion)report_motion(*motion,initial->frame);
    for(int index:args.hidden_primitives) {
        if(!initial->scene||std::size_t(index)>=initial->visible_primitives.size())fail("--hide-primitive fora da cena glTF inicial");
        initial->visible_primitives[std::size_t(index)]=false;
        std::cout<<"HIDDEN_PRIMITIVE "<<index<<'\n';
    }
    if(args.inspect_only)return 0;
    std::unique_ptr<mh4u::net::SocketSystem> sockets;
    std::unique_ptr<mh4u::net::WorldClient> network;
    if(args.connect_port) {
        const auto fresh=mh4u::net::identify_world(args.world,{world->config.stage,world->config.actor,world->config.floor,world->config.walls});
        if(fresh.document!=world->network_identity->document||mh4u::load_world_config(args.world)!=world->config)fail("Recursos de rede alterados durante o carregamento");
        sockets=std::make_unique<mh4u::net::SocketSystem>();
        network=std::make_unique<mh4u::net::WorldClient>(static_cast<std::uint16_t>(args.connect_port),args.player_name,world->network_identity->content);
        std::cout<<"NETWORK_CONTENT "<<world->network_identity->content.hex<<'\n';
    }
    SDLSession session; Window window; Gui gui(window); Pad pad; InputTrace trace(args.trace_input);
    // This owner is declared after the GL context, so GPU cleanup precedes context destruction.
    auto active=std::move(initial);active->upload();
    auto active_stage=std::move(stage);
    if(active_stage)active_stage->upload();
    int animation=active->initial_animation;
    const auto preferences=controls_path();
    Settings stored;
    Controls controls;
    std::string status;
    try { stored=settings_read(preferences); controls=load_controls(stored); }
    catch(const std::exception& e) { status=e.what(); }
    std::cout<<"OPENGL "<<glGetString(GL_VERSION)<<"\nRENDERER "<<glGetString(GL_RENDERER)<<'\n';
    Camera camera;
    if(world){camera.pitch=0.25f;camera.distance=3.3f;}
    bool running=true,wireframe=false,lighting=true,flip_v=!active->scene,rotate=false,use_alpha=false;
    bool playing=!network&&!args.paused&&!args.fixed_time&&args.motion_ticks<0&&args.player_ticks<0,loop=motion?false:args.loop;
    float animation_seconds=motion?motion->current().joint_source_frame:args.time,playback_speed=1;
    double motion_accumulator=0;
    bool pose_dirty=false;
    int texture_mode=args.texture_mode>=0?args.texture_mode:((active->original||active->scene)?0:2),frame=0;
    float old_time=static_cast<float>(SDL_GetTicks()/1000.0);
    double movement_accumulator=0;
    double network_accumulator=0;
    std::uint64_t network_revision=0;
    bool follow_hunter=true;
    pad.refresh();
    while(running) {
        if(pending_resource>=0) {
            try {
                auto candidate=std::make_unique<LoadedResource>(catalog[pending_resource],-2,0,false,loop,texture_pack.get());
                candidate->upload();
                active.swap(candidate); // Candidate now owns the previous GL/CPU resources until scope exit.
                selected_resource=pending_resource;animation=active->initial_animation;animation_seconds=0;
                playing=!args.paused&&!args.fixed_time;pose_dirty=false;flip_v=!active->scene;use_alpha=false;
                if(texture_mode!=2&&!(texture_mode==1&&(active->alternative||(texture_pack&&active->scene))))texture_mode=(active->original||active->scene)?0:2;
                camera.reset();
                old_time=static_cast<float>(SDL_GetTicks()/1000.0);status="Recurso carregado: "+active->spec.label;
                std::cout<<"RESOURCE_LOADED index="<<selected_resource<<" label="<<active->spec.label<<'\n';active->report(0);
            } catch(const std::exception& e) {
                status=std::string("Falha ao abrir recurso; cena atual preservada: ")+e.what();
                std::cerr<<"RESOURCE_FAILED index="<<pending_resource<<" active="<<selected_resource<<" reason="<<e.what()<<'\n';
            }
            pending_resource=-1;
        }
        const auto& scene_data=active->scene;
        auto& scene_frame=active->frame;
        const auto& mesh=active->mesh();
        auto& camera_bounds=active->bounds;
        auto& renderer=*active->renderer;
        const auto& original=active->original;
        const auto& alternative=active->alternative;
        const GLuint original_id=active->original_id,alternative_id=active->alternative_id;
        auto& io=ImGui::GetIO();
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            const bool accepted=ImGui_ImplSDL3_ProcessEvent(&event);
            trace.event(event,accepted,window.window);
            if(event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID==SDL_GetWindowID(window.window))) running=false;
            if(event.type==SDL_EVENT_GAMEPAD_ADDED || event.type==SDL_EVENT_GAMEPAD_REMOVED) pad.refresh();
            if(event.type==SDL_EVENT_MOUSE_MOTION && (event.motion.state&SDL_BUTTON_LMASK) && !io.WantCaptureMouse) {
                camera.yaw-=event.motion.xrel*0.008f; camera.pitch+=event.motion.yrel*0.008f;
            }
            if(event.type==SDL_EVENT_MOUSE_WHEEL && !io.WantCaptureMouse) camera.distance*=std::exp(-event.wheel.y*0.12f);
            if(event.type==SDL_EVENT_KEY_DOWN && !io.WantCaptureKeyboard) {
                if(event.key.key==SDLK_ESCAPE) running=false;
                if(event.key.key==SDLK_R) camera.reset();
                if(motion&&!event.key.repeat) {
                    if(event.key.key==SDLK_SPACE) { playing=!playing;motion_accumulator=0; }
                    if(event.key.key==SDLK_PERIOD) { playing=false;motion_accumulator=0;motion->tick();pose_dirty=true; }
                    if(event.key.key==SDLK_HOME) { motion->reset();playing=false;motion_accumulator=0;pose_dirty=true; }
                }
                if(active->player&&!event.key.repeat) {
                    try {
                        if(event.key.key==SDLK_SPACE) { playing=!playing;motion_accumulator=0; }
                        if(event.key.key==SDLK_PERIOD) { playing=false;motion_accumulator=0;active->accept(active->player->step());pose_dirty=true; }
                        if(event.key.key==SDLK_HOME) { active->reset_player();playing=false;motion_accumulator=0;pose_dirty=true; }
                        if(event.key.key==SDLK_1||event.key.key==SDLK_3) { active->request_player(event.key.key==SDLK_1?1:3);pose_dirty=true; }
                        if(args.trace_input&&(event.key.key==SDLK_SPACE||event.key.key==SDLK_PERIOD||event.key.key==SDLK_HOME||event.key.key==SDLK_1||event.key.key==SDLK_3)) {
                            std::cout<<"PLAYER_CONTROL key="<<event.key.key<<" requested_state="<<active->player_requested_state<<" ticks="<<active->player->player().completed_steps()<<" playing="<<playing<<'\n';std::cout.flush();
                        }
                    }catch(const std::exception& error){playing=false;status=error.what();}
                }
            }
        }
        if(!running) break;
        const float now=static_cast<float>(SDL_GetTicks()/1000.0);
        const float dt=std::clamp(now-old_time,0.0f,0.05f); old_time=now;
        if(network) {
            network->pump();
            if(network->snapshot()&&network->snapshot()->revision!=network_revision) {
                const auto& snapshot=*network->snapshot();network_revision=snapshot.revision;
                for(const auto& peer:snapshot.participants)if(peer.id==network->id()) {
                    world->state=peer.state;world->last.state=peer.state;world->last.blocked=peer.blocked;
                }
                const int selected=snapshot.selection==0xffff?-1:static_cast<int>(snapshot.selection);
                if(selected!=animation||animation_seconds!=0){animation=selected;pose_dirty=true;}
                animation_seconds=0;playing=false;
                if(args.trace_input) {
                    std::cout<<"NETWORK_SNAPSHOT tick="<<snapshot.tick<<" revision="<<snapshot.revision<<" self="<<network->id()<<" peers="<<snapshot.participants.size()<<'\n';
                    for(const auto& peer:snapshot.participants)std::cout<<std::setprecision(17)<<"NETWORK_ACTOR id="<<peer.id<<" position="<<peer.state.position.x<<','<<peer.state.position.y<<','<<peer.state.position.z<<" yaw="<<peer.state.yaw<<" applied="<<peer.applied<<" blocked="<<peer.blocked<<'\n';
                    std::cout.flush();
                }
            }
        }
        if(motion) {
            if(playing) {
                // dt is capped at 50 ms: at most three ticks/frame, with no
                // unbounded catch-up after suspension. This is preview pacing.
                motion_accumulator+=dt;
                constexpr double step=1/mh4u::ActionScene::ticks_per_second;
                while(motion_accumulator>=step) { motion->tick();motion_accumulator-=step;pose_dirty=true; }
            }
            animation_seconds=motion->current().joint_source_frame;
        }
        if(active->player&&playing) {
            motion_accumulator+=dt;
            constexpr double step=1.0/60.0; // Explicit inspection pacing, not a recovered gameplay scheduler.
            try { while(motion_accumulator>=step) { active->accept(active->player->step());motion_accumulator-=step;pose_dirty=true; } }
            catch(const std::exception& error){playing=false;motion_accumulator=0;status=error.what();}
        }
        if(!motion&&!active->player&&scene_data&&animation>=0&&playing) {
            animation_seconds+=dt*playback_speed;
            if(!active->native)animation_seconds=mh4u::animation_time(animation_seconds,scene_data->animations[animation].duration,loop);
            if(!active->native&&!loop&&animation_seconds>=scene_data->animations[animation].duration) playing=false;
            pose_dirty=true;
        }
        if(!io.WantCaptureKeyboard) {
            const bool* keys=SDL_GetKeyboardState(nullptr);
            camera.yaw+=(float(keys[SDL_SCANCODE_RIGHT])-float(keys[SDL_SCANCODE_LEFT]))*dt*1.5f;
            camera.pitch+=(float(keys[SDL_SCANCODE_UP])-float(keys[SDL_SCANCODE_DOWN]))*dt*1.5f;
            if(!world)camera.distance*=std::exp((float(keys[SDL_SCANCODE_S])-float(keys[SDL_SCANCODE_W]))*dt);
        }
        if(pad.handle && SDL_GetWindowFlags(window.window)&SDL_WINDOW_INPUT_FOCUS) {
            camera.yaw+=axis(pad.handle,controls.axis_x,controls.deadzone)*controls.speed*dt*(controls.invert_x?-1:1);
            camera.pitch+=axis(pad.handle,controls.axis_y,controls.deadzone)*controls.speed*dt*(controls.invert_y?1:-1);
            if(SDL_GetGamepadButton(pad.handle,reset_buttons[controls.reset])) camera.reset();
            const float zoom=(SDL_GetGamepadAxis(pad.handle,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)-SDL_GetGamepadAxis(pad.handle,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))/32767.0f;
            camera.distance*=std::exp(zoom*dt);
        }
        if(rotate) camera.yaw+=dt*0.4f;
        camera.constrain();
        if(world) {
            double strafe=0,forward=0;
            const bool focused=(SDL_GetWindowFlags(window.window)&SDL_WINDOW_INPUT_FOCUS)!=0;
            if(focused&&!io.WantCaptureKeyboard) {
                const auto* keys=SDL_GetKeyboardState(nullptr);
                strafe=double(keys[SDL_SCANCODE_D])-double(keys[SDL_SCANCODE_A]);
                forward=double(keys[SDL_SCANCODE_W])-double(keys[SDL_SCANCODE_S]);
            }
            if(focused&&pad.handle&&!io.WantCaptureKeyboard) {
                strafe+=axis(pad.handle,controls.move_x,controls.deadzone);
                forward-=axis(pad.handle,controls.move_y,controls.deadzone);
            }
            const double magnitude=std::hypot(strafe,forward);
            if(magnitude>1){strafe/=magnitude;forward/=magnitude;}
            constexpr double fixed_step=1.0/120.0;
            if(network) {
                network_accumulator+=dt;
                if(network->ready()&&network_accumulator>=mh4u::net::world_tick_ms/1000.0) {
                    network_accumulator=0;
                    const double dx=strafe*std::cos(camera.yaw)-forward*std::sin(camera.yaw);
                    const double dz=-strafe*std::sin(camera.yaw)-forward*std::cos(camera.yaw);
                    // Truncation rounds toward zero, keeping the integer vector inside the protocol's unit disk.
                    const double turn=(dx!=0||dz!=0)?std::clamp(std::remainder(std::atan2(dx,dz)-world->state.yaw,2*pi)/(pi*0.05),-1.0,1.0):0;
                    network->motion({static_cast<int>(dx*1000),static_cast<int>(dz*1000),static_cast<int>(turn*1000)});
                }
            } else movement_accumulator+=dt;
            while(!network&&movement_accumulator>=fixed_step) {
                movement_accumulator-=fixed_step;
                if(strafe!=0||forward!=0) {
                    const double distance=world->config.speed*fixed_step;
                    const double dx=(strafe*std::cos(camera.yaw)-forward*std::sin(camera.yaw))*distance;
                    const double dz=(-strafe*std::sin(camera.yaw)-forward*std::cos(camera.yaw))*distance;
                    world->step(dx,dz,std::remainder(std::atan2(dx,dz)-world->state.yaw,2*pi));
                }
            }
            if(follow_hunter) {
                camera_bounds.center={static_cast<float>(world->state.position.x),
                    static_cast<float>(world->state.position.y+world->config.height*0.55),static_cast<float>(world->state.position.z)};
                camera_bounds.radius=static_cast<float>(world->config.height*0.55);
            }
        }
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();
        trace.frame(playing,animation);
        const float panel=std::min(350.0f,io.DisplaySize.x*0.42f);
        ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({panel,io.DisplaySize.y});
        ImGui::Begin("Ferramentas",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored({0.48f,0.78f,0.69f,1},"MH4U NATIVE");
        ImGui::TextWrapped("%s",motion?"Inspeção de ação pareada":network?"Cena compartilhada local":world?"Cena controlável":"Laboratório de recursos");
        ImGui::TextDisabled("%s",world?"Movimento de diagnóstico":"Protótipo de renderização");
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        if(network&&ImGui::CollapsingHeader("Conexão",ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("%s",network->done()?(network->error().empty()?"Desconectado":network->error().c_str()):network->ready()?"Conectado ao servidor local":"Conectando...");
            if(network->snapshot()) {
                const auto& snapshot=*network->snapshot();
                ImGui::Text("Caçadores: %zu · atualização %llu",snapshot.participants.size(),static_cast<unsigned long long>(snapshot.tick));
                for(const auto& peer:snapshot.participants)ImGui::Text("%s%s · %.1f / %.1f",peer.name.c_str(),peer.id==network->id()?" (você)":"",peer.state.position.x,peer.state.position.z);
            }
            ImGui::TextWrapped("Posições e colisão calculadas pelo servidor. Combate cooperativo e acesso pela internet ainda pendentes.");
        }
        if(world&&ImGui::CollapsingHeader("Caçador no cenário",ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("WASD / analógico esquerdo: mover. Mouse / analógico direito: câmera.");
            ImGui::Text("Posição %.2f, %.2f, %.2f",world->state.position.x,world->state.position.y,world->state.position.z);
            ImGui::TextWrapped("%s",world->last.blocked?"Movimento limitado pela colisão.":"Personagem apoiado no piso.");
            ImGui::Checkbox("Câmera segue o caçador",&follow_hunter);
            ImGui::BeginDisabled(bool(network));
            if(ImGui::Button("Voltar ao ponto de diagnóstico",{-1,0})) {
                world->state=world->initial;world->last={};world->last.state=world->state;movement_accumulator=0;
            }
            ImGui::EndDisabled();
            ImGui::TextWrapped("Velocidade, ponto inicial e pose são de inspeção. Física e ações originais ainda pendentes.");
        }
        if(texts&&ImGui::CollapsingHeader("Textos do jogo",ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Português do Brasil",&translated);
            const auto& identities=texts->identities();
            ImGui::SetNextItemWidth(-1);
            if(ImGui::BeginCombo("##text_document",identities[text_document].document.c_str())) {
                for(std::size_t i=0;i<identities.size();++i)
                    if(ImGui::Selectable(identities[i].document.c_str(),text_document==static_cast<int>(i))) { text_document=static_cast<int>(i);text_fragment=0; }
                ImGui::EndCombo();
            }
            const auto& identity=identities[text_document];
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderInt("##text_fragment",&text_fragment,0,static_cast<int>(identity.fragment_count)-1,"Fragmento: %d");
            const auto result=texts->lookup(identity,static_cast<std::uint32_t>(text_fragment),translated?mh4u::localization::Language::translated:mh4u::localization::Language::english);
            if(const auto* view=std::get_if<mh4u::localization::TextView>(&result)) {
                ImGui::PushTextWrapPos(0);ImGui::TextUnformatted(view->text.data(),view->text.data()+view->text.size());ImGui::PopTextWrapPos();
                ImGui::TextDisabled("%s",view->translated?"Tradução experimental":"Texto original em inglês");
            } else ImGui::TextWrapped("%s",std::get<mh4u::localization::Error>(result).message.c_str());
            ImGui::TextWrapped("%zu traduções / %zu fragmentos. Comandos e layout do jogo ainda pendentes.",texts->translated_count(),texts->fragment_count());
            ImGui::Separator();
        }
        if(!args.catalog.empty()) {
            ImGui::SetNextItemWidth(-1);
            if(ImGui::BeginCombo("##resource",catalog[selected_resource].label.c_str())) {
                for(std::size_t i=0;i<catalog.size();++i) {
                    ImGui::PushID(static_cast<int>(i));
                    if(ImGui::Selectable(catalog[i].label.c_str(),selected_resource==static_cast<int>(i)))pending_resource=static_cast<int>(i);
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("Recurso para inspeção");
            if(!status.empty())ImGui::TextWrapped("%s",status.c_str());
        }
        ImGui::TextWrapped("%s",path_text(active->spec.model.filename()).c_str());
        ImGui::Text("%zu vértices · %zu triângulos",mesh.source_positions,mesh.triangles.size()/3);
        ImGui::TextWrapped("Câmera de inspeção do recurso extraído");
        if(mesh.material_groups>1&&!scene_data) ImGui::TextWrapped("Atenção: uma textura é aplicada a todos os materiais nesta versão.");
        if(scene_data) {
            if(active->native||active->player) {
                ImGui::Text("%zu ossos · %zu partes",active->native_joints,active->native_parts);
                ImGui::TextWrapped("Relógio da animação · reprodução a %.0f ticks/s assumidos",active->player?60.0:active->native_rate);
                ImGui::Text("Frame %.1f · tick %llu",active->native_frame,static_cast<unsigned long long>(active->native_tick));
                ImGui::TextUnformatted("Eventos da animação");
                ImGui::Text("G0 %08X · G1 %08X",active->native_events[0],active->native_events[1]);
                ImGui::Text("G2 %08X · G3 %08X",active->native_events[2],active->native_events[3]);
            } else ImGui::Text("%zu nós · %zu skins",scene_data->nodes.size(),scene_data->skins.size());
            if(ImGui::CollapsingHeader("Animação",ImGuiTreeNodeFlags_DefaultOpen)) {
                if(active->player) {
                    ImGui::TextUnformatted("Controlador do jogador em inspeção");
                    if(active->player_requested_state<0)ImGui::TextUnformatted("Estado de referência");
                    else ImGui::Text("Último pedido de estado: %d",active->player_requested_state);
                    ImGui::Text("Passos: %llu",static_cast<unsigned long long>(active->player->player().completed_steps()));
                    try {
                        if(ImGui::Button(playing?"Pausar":"Reproduzir")) { playing=!playing;motion_accumulator=0; }
                        ImGui::SameLine();if(ImGui::Button("Um passo")) { playing=false;motion_accumulator=0;active->accept(active->player->step());pose_dirty=true; }
                        if(ImGui::Button("Estado 1")) { active->request_player(1);pose_dirty=true; }
                        ImGui::SameLine();if(ImGui::Button("Estado 3")) { active->request_player(3);pose_dirty=true; }
                        if(ImGui::Button("Reiniciar inspeção")) { active->reset_player();playing=false;motion_accumulator=0;pose_dirty=true; }
                    }catch(const std::exception& error){playing=false;status=error.what();}
                    ImGui::TextWrapped("Espaço: pausa. Ponto: um passo. 1/3: pedir estado. Home: reiniciar.");
                    ImGui::TextWrapped("Pedidos manuais; entrada, inicialização e ritmo originais ainda pendentes.");
                }
                if(!motion&&!active->player) {
                ImGui::BeginDisabled(bool(network));
                const char* selected=animation>=0?scene_data->animations[animation].name.c_str():"Pose de referência";
                ImGui::SetNextItemWidth(-1);
                if(ImGui::BeginCombo("##animation",selected)) {
                    if(ImGui::Selectable("Pose de referência",animation<0)) { animation=-1; pose_dirty=true; }
                    for(std::size_t i=0;i<scene_data->animations.size();++i)
                        if(ImGui::Selectable(scene_data->animations[i].name.c_str(),animation==static_cast<int>(i))) {
                            try {
                                if(active->native&&active->native->supports_transitions())active->evaluate(static_cast<int>(i),0,loop,true);
                                animation=static_cast<int>(i);animation_seconds=0;pose_dirty=true;
                            }catch(const std::exception& error){status=std::string("Não foi possível trocar a ação: ")+error.what();}
                        }
                    ImGui::EndCombo();
                }
                if(active->native&&active->native->supports_transitions())ImGui::TextWrapped("Trocar de ação preserva a pose para a transição. A linha do tempo permite buscar uma pose isolada.");
                if(animation>=0) {
                    if(ImGui::Button(playing?"Pausar":"Reproduzir")) { if(!playing&&active->native&&active->native_ended){animation_seconds=0;pose_dirty=true;} playing=!playing; trace.playback(playing); }
                    ImGui::SameLine(); if(ImGui::Checkbox(active->native?"Loop da ação":"Repetir",&loop)) { animation_seconds=0;pose_dirty=true; }
                    ImGui::SetNextItemWidth(-1);
                    if(ImGui::SliderFloat("##time",&animation_seconds,0,scene_data->animations[animation].duration,"Tempo: %.3f s")) {
                        playing=false;pose_dirty=true;
                        if(active->native)active->evaluate(animation,animation_seconds,loop,false,true);
                    }
                    ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##rate",&playback_speed,0.1f,2,"Velocidade: %.2fx");
                } else ImGui::TextWrapped(scene_data->animations.empty()?"Nenhuma animação carregada.":"Transformações de referência da cena.");
                ImGui::EndDisabled();
                if(network)ImGui::TextWrapped("Pose compartilhada de inspeção. Ações e tempo de animação ainda não sincronizados.");
                }
                if(motion) {
                    ImGui::Text("Ação %u:%u · clip %u",motion->action().identity.entry_id,motion->action().identity.slot,motion->action().gltf_clip_index);
                    ImGui::TextWrapped("Política diagnóstica: 1 frame/tick a 60 ticks/s assumidos; ritmo original desconhecido.");
                    ImGui::Text("Frame fonte %.1f · tick %llu",motion->current().joint_source_frame,static_cast<unsigned long long>(motion->ticks()));
                    if(motion->events_bound()) {
                        const auto& events=motion->current().state.pre.clock.event_masks;
                        ImGui::TextUnformatted("Eventos da animação · leitura");
                        ImGui::Text("G0 %08X · G1 %08X",events[0],events[1]);
                        ImGui::Text("G2 %08X · G3 %08X",events[2],events[3]);
                    }
                    ImGui::TextWrapped("Espaço: pausa. Ponto: um tick. Home: reinicia o estado. Câmera acompanha o modelo.");
                    ImGui::TextDisabled("%s",playing?"Reproduzindo":"Pausado");
                }
            }
        }
        ImGui::Spacing();
        if(ImGui::CollapsingHeader("Texturas",ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BeginDisabled(!original_id&&!scene_data); ImGui::RadioButton(scene_data?"Texturas do modelo":"Original",&texture_mode,0); ImGui::EndDisabled();
            ImGui::BeginDisabled(!alternative_id&&!(texture_pack&&scene_data)); ImGui::RadioButton(texture_pack&&scene_data?texture_pack->name.c_str():"Alternativa",&texture_mode,1); ImGui::EndDisabled();
            ImGui::RadioButton("Sem textura",&texture_mode,2);
            if(texture_pack&&scene_data) {
                ImGui::TextWrapped("Pack: %zu de %zu imagens substituídas. As demais usam o original.",active->pack_matches,scene_data->images.size());
            } else if(!alternative_id) ImGui::TextWrapped("Nenhuma textura alternativa carregada.");
            ImGui::Checkbox("Inverter coordenada V",&flip_v);
            ImGui::BeginDisabled(scene_data||((!original || original->alpha.empty()) && (!alternative || alternative->alpha.empty())));
            ImGui::Checkbox("Transparência por alpha",&use_alpha);
            ImGui::TextDisabled("Uso experimental");
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("A função original do canal alpha ainda é desconhecida.");
            ImGui::EndDisabled();
        }
        if(scene_data&&ImGui::CollapsingHeader("Partes da geometria")) {
            ImGui::TextWrapped("Visibilidade de inspeção. A seleção original do jogo ainda é desconhecida.");
            if(ImGui::Button("Mostrar todas",{-1,0}))std::fill(active->visible_primitives.begin(),active->visible_primitives.end(),true);
            for(std::size_t i=0;i<scene_data->primitives.size();++i) {
                ImGui::PushID(static_cast<int>(i));
                bool visible=active->visible_primitives[i];
                if(ImGui::Checkbox("##visible",&visible))active->visible_primitives[i]=visible;
                ImGui::SameLine();ImGui::Text("Primitiva %zu",i);
                const int material=scene_data->primitives[i].material;
                if(material>=0&&ImGui::IsItemHovered())ImGui::SetTooltip("%s",scene_data->materials[material].name.c_str());
                ImGui::PopID();
            }
        }
        if(ImGui::CollapsingHeader("Visualização",ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Malha em linhas",&wireframe); ImGui::Checkbox("Iluminação de inspeção",&lighting);
            ImGui::Checkbox("Girar modelo",&rotate);
            if(ImGui::Button("Enquadrar modelo",{-1,0})) { camera.reset(); if(motion)camera_bounds=motion->transformed_bounds(scene_frame);else { camera_bounds.center=mesh.center; camera_bounds.radius=mesh.radius; } }
            ImGui::TextWrapped("%s",world?"Arraste para girar. Roda: zoom. Setas: órbita. WASD: mover.":"Arraste para girar. Roda: zoom. Setas: órbita. W/S: zoom. R: enquadrar.");
        }
        if(ImGui::CollapsingHeader("Controle / joystick",ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("%s",pad.handle?SDL_GetGamepadName(pad.handle):"Nenhum controle conectado");
            const char* axes="Esquerdo X\0Esquerdo Y\0Direito X\0Direito Y\0";
            ImGui::SetNextItemWidth(-1); ImGui::Combo("##horizontal",&controls.axis_x,axes); ImGui::TextDisabled("Eixo horizontal");
            ImGui::SetNextItemWidth(-1); ImGui::Combo("##vertical",&controls.axis_y,axes); ImGui::TextDisabled("Eixo vertical");
            if(world) {
                ImGui::SetNextItemWidth(-1);ImGui::Combo("##move_x",&controls.move_x,axes);ImGui::TextDisabled("Movimento lateral");
                ImGui::SetNextItemWidth(-1);ImGui::Combo("##move_y",&controls.move_y,axes);ImGui::TextDisabled("Movimento frente / trás");
            }
            ImGui::Checkbox("Inverter horizontal",&controls.invert_x); ImGui::Checkbox("Inverter vertical",&controls.invert_y);
            ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##deadzone",&controls.deadzone,0,0.5f,"Zona morta: %.2f");
            ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##speed",&controls.speed,0.2f,5,"Sensibilidade: %.1f");
            ImGui::SetNextItemWidth(-1); ImGui::Combo("##reset",&controls.reset,"Sul / A / Cross\0Leste / B / Circle\0Oeste / X / Square\0Norte / Y / Triangle\0");
            ImGui::TextWrapped("Botão para enquadrar · gatilhos: zoom");
            if(ImGui::Button("Salvar controles",{-1,0})) {
                try { save_controls(preferences,settings_read(preferences),controls); stored=settings_read(preferences); status="Controles salvos."; }
                catch(const std::exception& e) { status=e.what(); }
            }
        }
        if(args.catalog.empty()&&!status.empty()) ImGui::TextWrapped("%s",status.c_str());
        ImGui::End();
        const auto* viewport=ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({panel+24,viewport->Size.y-20},0,{0,1});
        ImGui::SetNextWindowBgAlpha(0.72f);
        ImGui::Begin("Estado",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Recurso real da ROM · motor gráfico nativo");
        ImGui::TextDisabled("%s",network?"Movimento compartilhado local · combate pendente":world?"Colisão diagnóstica · combate e online pendentes":"Combate e coop jogável ainda pendentes");
        ImGui::End();
        ImGui::Render();
        if(scene_data&&pose_dirty) {
            if(motion)motion->evaluate(*scene_data,scene_frame);
            else if(!active->player)active->evaluate(animation,animation_seconds,loop);
            renderer.upload(mesh,true); pose_dirty=false;
            if(motion)camera_bounds.center=motion->transformed_bounds(scene_frame).center;
            if(active->native){if(!world)camera_bounds.center=mesh.center;if(active->native_ended)playing=false;}
            if(active->player&&!world)camera_bounds.center=mesh.center;
        }
        int width=0,height=0; SDL_GetWindowSizeInPixels(window.window,&width,&height);
        if(width<=0 || height<=0) { SDL_Delay(16); continue; }
        glViewport(0,0,width,height); glClearColor(0.13f,0.16f,0.18f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        const int side=std::clamp(static_cast<int>(panel*io.DisplayFramebufferScale.x),0,width-1);
        glViewport(side,0,width-side,height);
        const auto view_projection=camera.matrix(camera_bounds,float(width-side)/height);
        if(active_stage)active_stage->draw(view_projection,mh4u::identity_matrix(),texture_mode,false,lighting,wireframe,false);
        if(network&&network->snapshot()) {
            for(const auto& peer:network->snapshot()->participants)active->draw(view_projection,world->actor_transform(peer.state),texture_mode,flip_v,lighting,wireframe,use_alpha);
        } else active->draw(view_projection,motion?motion->model_matrix():world?world->actor_transform():mh4u::identity_matrix(),texture_mode,flip_v,lighting,wireframe,use_alpha);
        glViewport(0,0,width,height); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ++frame;
        if(args.frames && frame>=args.frames) {
            if(!args.screenshot.empty()) { screenshot(args.screenshot,width,height); std::cout<<"SCREENSHOT "<<path_text(args.screenshot)<<'\n'; }
            running=false;
        }
        check(SDL_GL_SwapWindow(window.window),"Apresentar quadro");
    }
    const auto& mesh=active->mesh();
    if(active->scene) std::cout<<"FINAL_TIME "<<animation_seconds<<"\nFINAL_BOUNDS "<<mesh.minimum.x<<' '<<mesh.minimum.y<<' '<<mesh.minimum.z
                           <<" / "<<mesh.maximum.x<<' '<<mesh.maximum.y<<' '<<mesh.maximum.z<<'\n';
    if(motion)report_motion(*motion,active->frame);
    if(active->player)active->report(0);
    if(world)world->report();
    if(network) {
        network->close();
        while(!network->done()){network->pump();SDL_Delay(1);}
        std::cout<<"NETWORK_CLOSED confirmed="<<network->confirmed_bye()<<" reason="<<network->error()<<'\n';
    }
    std::cout<<"FRAMES "<<frame<<'\n'; return 0;
}
}

int main(int argc,char** argv) {
    try { return run(options(argc,argv)); }
    catch(const std::exception& e) { std::cerr<<"Erro: "<<e.what()<<'\n'; return 1; }
}

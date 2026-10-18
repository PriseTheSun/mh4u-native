#include "scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mh4u {
namespace {
[[noreturn]] void fail(const char* reason) { throw std::runtime_error(reason); }
bool finite(Vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
Vec3 normalized(Vec3 v) {
    const float length=std::hypot(v.x,v.y,v.z);
    if(!std::isfinite(length)||length<1e-12f) fail("Normal degenerada na cena");
    return {v.x/length,v.y/length,v.z/length};
}
void check_matrix(const Matrix& m) {
    for(float value:m) if(!std::isfinite(value)) fail("Matriz não finita na cena");
    if(std::abs(m[3])+std::abs(m[7])+std::abs(m[11])+std::abs(m[15]-1)>1e-4f) fail("Transformação de cena não afim");
}
}
Matrix identity_matrix() { return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}; }
Matrix matrix_product(const Matrix& a,const Matrix& b) {
    Matrix result{};
    for(int col=0;col<4;++col) for(int row=0;row<4;++row) for(int k=0;k<4;++k)
        result[col*4+row]+=a[k*4+row]*b[col*4+k];
    return result;
}
Vec3 transform_point(const Matrix& m,Vec3 p) {
    Vec3 result{m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};
    if(!finite(result)) fail("Posição transformada não finita");
    return result;
}
Vec3 transform_normal(const Matrix& m,Vec3 n) {
    // Cofactor matrix / determinant is inverse-transpose, including nonuniform scale.
    const float a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
    const float A=e*i-f*h,B=f*g-d*i,C=d*h-e*g;
    const float determinant=a*A+b*B+c*C;
    if(!std::isfinite(determinant)||std::abs(determinant)<1e-12f) fail("Transformação singular não permite transformar normais");
    return normalized({(A*n.x+B*n.y+C*n.z)/determinant,
                       ((c*h-b*i)*n.x+(a*i-c*g)*n.y+(b*g-a*h)*n.z)/determinant,
                       ((b*f-c*e)*n.x+(c*d-a*f)*n.y+(a*e-b*d)*n.z)/determinant});
}
Quaternion normalized_quaternion(Quaternion q) {
    double length=0;
    for(float value:q) { if(!std::isfinite(value)) fail("Quaternion não finito"); length+=double(value)*value; }
    if(length<1e-20) fail("Quaternion nulo");
    const float inverse=static_cast<float>(1/std::sqrt(length));
    for(float& value:q) value*=inverse;
    return q;
}
Quaternion quaternion_slerp(Quaternion a,Quaternion b,float t) {
    a=normalized_quaternion(a); b=normalized_quaternion(b);
    float cosine=0; for(int i=0;i<4;++i) cosine+=a[i]*b[i];
    if(cosine<0) { for(float& value:b) value=-value; cosine=-cosine; }
    cosine=std::clamp(cosine,0.0f,1.0f);
    float left=1-t,right=t;
    if(cosine<0.9995f) {
        const float angle=std::acos(cosine),denominator=std::sin(angle);
        left=std::sin((1-t)*angle)/denominator; right=std::sin(t*angle)/denominator;
    }
    Quaternion result{}; for(int i=0;i<4;++i) result[i]=left*a[i]+right*b[i];
    return normalized_quaternion(result);
}
Matrix transform_matrix(const Transform& t) {
    if(!finite(t.translation)||!finite(t.scale)) fail("TRS não finito");
    const auto q=normalized_quaternion(t.rotation); const float x=q[0],y=q[1],z=q[2],w=q[3];
    return {(1-2*(y*y+z*z))*t.scale.x,2*(x*y+z*w)*t.scale.x,2*(x*z-y*w)*t.scale.x,0,
            2*(x*y-z*w)*t.scale.y,(1-2*(x*x+z*z))*t.scale.y,2*(y*z+x*w)*t.scale.y,0,
            2*(x*z+y*w)*t.scale.z,2*(y*z-x*w)*t.scale.z,(1-2*(x*x+y*y))*t.scale.z,0,
            t.translation.x,t.translation.y,t.translation.z,1};
}
std::array<float,4> normalized_weights(std::array<float,4> weights) {
    double sum=0;
    for(float w:weights) { if(!std::isfinite(w)||w<0) fail("Peso de skin negativo ou não finito"); sum+=w; }
    if(sum<1e-12) fail("Vértice skinned sem peso");
    for(float& w:weights) w=static_cast<float>(w/sum);
    return weights;
}
void prepare_scene(Scene& scene) {
    if(scene.nodes.empty()||scene.nodes.size()>65536) fail("Quantidade de nós não suportada");
    scene.hierarchy_order.clear();
    std::vector<unsigned char> state(scene.nodes.size(),0);
    for(std::size_t start=0;start<scene.nodes.size();++start) {
        std::vector<std::size_t> chain;
        int current=static_cast<int>(start);
        while(current>=0 && state[std::size_t(current)]!=2) {
            if(state[std::size_t(current)]==1) fail("Ciclo na hierarquia de cena");
            auto& node=scene.nodes[std::size_t(current)];
            if(node.parent<-1||node.parent>=static_cast<int>(scene.nodes.size())) fail("Pai de nó fora do intervalo");
            state[std::size_t(current)]=1; chain.push_back(std::size_t(current)); current=node.parent;
        }
        for(auto it=chain.rbegin();it!=chain.rend();++it) {
            auto& node=scene.nodes[*it];
            if(node.has_matrix) check_matrix(node.matrix); else node.rest.rotation=normalized_quaternion(node.rest.rotation);
            scene.hierarchy_order.push_back(*it); state[*it]=2;
        }
    }
    for(const auto& skin:scene.skins) {
        if(skin.joints.empty()||skin.joints.size()!=skin.inverse_bind.size()) fail("Skin incompleta");
        for(auto joint:skin.joints) if(joint>=scene.nodes.size()) fail("Junta fora da cena");
        for(const auto& m:skin.inverse_bind) check_matrix(m);
    }
    std::size_t total=0;
    for(auto& primitive:scene.primitives) {
        if(primitive.indices.empty()||primitive.indices.size()%3) fail("Primitiva não triangular");
        for(auto index:primitive.indices) if(index>=primitive.vertices.size()) fail("Índice de vértice fora da primitiva");
        if(primitive.material<-1||primitive.material>=static_cast<int>(scene.materials.size())) fail("Material fora do intervalo");
        for(auto& vertex:primitive.vertices) {
            if(!finite(vertex.vertex.position)||!std::isfinite(vertex.vertex.uv.x)||!std::isfinite(vertex.vertex.uv.y)) fail("Vértice não finito");
            vertex.vertex.normal=normalized(vertex.vertex.normal);
            if(primitive.skinned) vertex.weights=normalized_weights(vertex.weights);
        }
    }
    for(const auto& instance:scene.instances) {
        if(instance.primitive>=scene.primitives.size()||instance.node>=scene.nodes.size()||instance.skin<-1||instance.skin>=static_cast<int>(scene.skins.size())) fail("Instância de cena inválida");
        const auto& primitive=scene.primitives[instance.primitive];
        total+=primitive.indices.size();
        if(total>2U*1024U*1024U) fail("Cena excede orçamento de triângulos expandidos");
        if(primitive.skinned!=(instance.skin>=0)) fail("Atributos de skin e instância incompatíveis");
        if(instance.skin>=0) for(const auto& vertex:primitive.vertices) for(int j=0;j<4;++j)
            if(vertex.weights[j]>0 && vertex.joints[j]>=scene.skins[instance.skin].joints.size()) fail("JOINTS_0 fora da skin");
    }
    for(auto& animation:scene.animations) {
        animation.duration=0;
        std::vector<std::array<bool,3>> occupied(scene.nodes.size());
        for(auto& channel:animation.channels) {
            if(channel.node>=scene.nodes.size()||scene.nodes[channel.node].has_matrix) fail("Canal deve apontar para nó TRS");
            auto& used=occupied[channel.node][static_cast<std::size_t>(channel.path)];
            if(used) fail("Canais duplicados para o mesmo TRS"); used=true;
            if(channel.times.empty()||channel.times.size()!=channel.values.size()) fail("Canal de animação incompleto");
            const bool cubic=channel.interpolation==Interpolation::cubic_spline;
            if(cubic) {
                if(channel.times.size()<2||channel.in_tangents.size()!=channel.times.size()||channel.out_tangents.size()!=channel.times.size())
                    fail("Canal CUBICSPLINE requer ao menos duas chaves e tangentes completas");
            } else if(!channel.in_tangents.empty()||!channel.out_tangents.empty()) fail("Tangentes em canal sem CUBICSPLINE");
            float previous=-1;
            for(std::size_t k=0;k<channel.times.size();++k) {
                const float time=channel.times[k];
                if(!std::isfinite(time)||time<0||time<=previous) fail("Tempos de animação devem ser crescentes e não negativos");
                previous=time;
                for(float value:channel.values[k]) if(!std::isfinite(value)) fail("Valor de animação não finito");
                if(cubic) {
                    for(float value:channel.in_tangents[k]) if(!std::isfinite(value)) fail("Tangente de animação não finita");
                    for(float value:channel.out_tangents[k]) if(!std::isfinite(value)) fail("Tangente de animação não finita");
                }
                if(channel.path==ChannelPath::rotation) {
                    if(cubic) {
                        // Scaling a vertex alone changes the curve relative to its tangents.
                        // Validate unit vertices without silently rewriting spline data.
                        double squared=0; for(float value:channel.values[k]) squared+=double(value)*value;
                        if(std::abs(squared-1)>1e-4) fail("Quaternion CUBICSPLINE deve ser unitário");
                    } else channel.values[k]=normalized_quaternion(channel.values[k]);
                }
            }
            animation.duration=std::max(animation.duration,channel.times.back());
        }
    }
}
float animation_time(float time,float duration,bool loop) {
    if(!std::isfinite(time)||!std::isfinite(duration)||duration<0) fail("Tempo de animação inválido");
    if(duration==0) return 0;
    if(!loop) return std::clamp(time,0.0f,duration);
    float result=std::fmod(time,duration); if(result<0) result+=duration; return result;
}
std::vector<Transform> sample_animation(const Scene& scene,int animation,float time,bool loop) {
    std::vector<Transform> pose; pose.reserve(scene.nodes.size());
    for(const auto& node:scene.nodes) pose.push_back(node.rest);
    if(animation==-1) return pose;
    if(animation<0||animation>=static_cast<int>(scene.animations.size())) fail("Animação fora do intervalo");
    const auto& clip=scene.animations[animation]; const float sampled=animation_time(time,clip.duration,loop);
    for(const auto& channel:clip.channels) {
        auto upper=std::upper_bound(channel.times.begin(),channel.times.end(),sampled);
        std::size_t a=upper==channel.times.begin()?0:std::size_t(upper-channel.times.begin()-1);
        const std::size_t b=std::min(a+1,channel.times.size()-1);
        float t=0;
        if(b!=a&&channel.interpolation!=Interpolation::step) t=std::clamp((sampled-channel.times[a])/(channel.times[b]-channel.times[a]),0.0f,1.0f);
        std::array<float,4> value{};
        if(channel.interpolation==Interpolation::cubic_spline) {
            const float t2=t*t,t3=t2*t,dt=channel.times[b]-channel.times[a];
            for(int i=0;i<(channel.path==ChannelPath::rotation?4:3);++i)
                value[i]=(2*t3-3*t2+1)*channel.values[a][i]+dt*(t3-2*t2+t)*channel.out_tangents[a][i]
                         +(-2*t3+3*t2)*channel.values[b][i]+dt*(t3-t2)*channel.in_tangents[b][i];
            // Unlike LINEAR/slerp, spline signs and tangent magnitudes are authored data.
            if(channel.path==ChannelPath::rotation) value=normalized_quaternion(value);
        } else if(channel.path==ChannelPath::rotation) value=quaternion_slerp(channel.values[a],channel.values[b],t);
        else for(int i=0;i<3;++i) value[i]=channel.values[a][i]*(1-t)+channel.values[b][i]*t;
        auto& transform=pose[channel.node];
        if(channel.path==ChannelPath::translation) transform.translation={value[0],value[1],value[2]};
        else if(channel.path==ChannelPath::scale) transform.scale={value[0],value[1],value[2]};
        else transform.rotation=value;
    }
    return pose;
}
void evaluate_scene(const Scene& scene,int animation,float time,bool loop,SceneFrame& frame) {
    const auto pose=sample_animation(scene,animation,time,loop);
    frame.global_transforms.resize(scene.nodes.size());
    for(auto index:scene.hierarchy_order) {
        const auto& node=scene.nodes[index]; const auto local=node.has_matrix?node.matrix:transform_matrix(pose[index]);
        frame.global_transforms[index]=node.parent<0?local:matrix_product(frame.global_transforms[node.parent],local);
    }
    frame.mesh.triangles.clear(); frame.draws.clear(); frame.mesh.source_positions=0;
    for(const auto& instance:scene.instances) {
        const auto& primitive=scene.primitives[instance.primitive];
        std::vector<Matrix> skin_matrices;
        if(instance.skin>=0) {
            const auto& skin=scene.skins[instance.skin]; skin_matrices.reserve(skin.joints.size());
            for(std::size_t j=0;j<skin.joints.size();++j) skin_matrices.push_back(matrix_product(frame.global_transforms[skin.joints[j]],skin.inverse_bind[j]));
        }
        std::vector<Vertex> vertices; vertices.reserve(primitive.vertices.size());
        for(const auto& source:primitive.vertices) {
            Matrix transform=frame.global_transforms[instance.node];
            if(instance.skin>=0) {
                transform.fill(0);
                for(int j=0;j<4;++j) if(source.weights[j]>0)
                    for(int k=0;k<16;++k) transform[k]+=skin_matrices[source.joints[j]][k]*source.weights[j];
            }
            vertices.push_back({transform_point(transform,source.vertex.position),transform_normal(transform,source.vertex.normal),source.vertex.uv});
        }
        frame.draws.push_back({frame.mesh.triangles.size(),primitive.indices.size(),primitive.material,instance.primitive});
        for(auto index:primitive.indices) frame.mesh.triangles.push_back(vertices[index]);
        frame.mesh.source_positions+=primitive.vertices.size();
    }
    if(frame.mesh.triangles.empty()) fail("Cena sem triângulos visíveis");
    frame.mesh.minimum=frame.mesh.maximum=frame.mesh.triangles.front().position;
    for(const auto& vertex:frame.mesh.triangles) {
        auto p=vertex.position;
        frame.mesh.minimum={std::min(frame.mesh.minimum.x,p.x),std::min(frame.mesh.minimum.y,p.y),std::min(frame.mesh.minimum.z,p.z)};
        frame.mesh.maximum={std::max(frame.mesh.maximum.x,p.x),std::max(frame.mesh.maximum.y,p.y),std::max(frame.mesh.maximum.z,p.z)};
    }
    const auto low=frame.mesh.minimum,high=frame.mesh.maximum;
    frame.mesh.center={low.x*0.5f+high.x*0.5f,low.y*0.5f+high.y*0.5f,low.z*0.5f+high.z*0.5f};
    frame.mesh.radius=std::hypot(high.x-low.x,high.y-low.y,high.z-low.z)*0.5f;
    if(!std::isfinite(frame.mesh.radius)||frame.mesh.radius<1e-8f) fail("Extensão de cena inválida");
    frame.mesh.material_groups=frame.draws.size(); frame.mesh.has_uv=true;
}
}

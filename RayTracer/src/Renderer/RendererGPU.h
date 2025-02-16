#include "Renderer.h"

#define KOMPUTE_DISABLE_VK_DEBUG_LAYERS
#include <kompute/Kompute.hpp>

struct PushConsts
{
    glm::mat4 viewMatrix;
    glm::mat4 inverseProjectionMatrix;
    glm::vec3 camPos;
    uint32_t frameIndex;
};

class RendererGPU : public Renderer {
public:
    void InitGPU(Scene* scene, BVH* bvh);
    void RenderGPU(Camera* camera);

    virtual bool OnResize(uint32_t width, uint32_t height) override;

private:
    std::vector<uint32_t> CompileShader(const std::string& filepath);
    std::vector<uint32_t> m_kp_shader;

    kp::Manager m_kp_manager;
    std::shared_ptr<kp::Algorithm> m_kp_algorithm;
    std::shared_ptr<kp::Sequence> m_kp_sequence;

    // Storage Buffers
    std::shared_ptr<kp::TensorT<float>> m_kp_imgRaw;
    std::vector<float> m_kp_consts;
    std::vector<PushConsts> m_kp_pushConsts;
    std::vector<std::shared_ptr<kp::Memory>>  m_kp_params;
};
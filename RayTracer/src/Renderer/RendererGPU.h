#pragma once

#include "Renderer.h"
#include <thread>

#define KOMPUTE_DISABLE_VK_DEBUG_LAYERS
#include <kompute/Kompute.hpp>

struct PushConsts
{
    // These have to be 16 byte aligned, that's why vec4 instead of vec3
    glm::vec4 camPos;
    glm::vec4 camRight;
    glm::vec4 camUp;
    uint32_t frameIndex;
    uint32_t renderMode;
    uint32_t bounces;
    float exposure;
    float dof_dist;
    float dof_strength;
    float skyX;
    float skyY;
    bool useACE;
};

class RendererGPU : public Renderer {
public:
    ~RendererGPU();

    void InitGPU(Scene* scene, BVH* bvh, Camera* cam);
    void RenderGPU(Camera* camera);

    virtual bool OnResize(uint32_t width, uint32_t height) override;
    virtual void ResetFrameIndex() override;
    virtual bool OnCameraMoved() override;

private:
    std::vector<uint32_t> CompileShader(const std::string& filepath);
    std::vector<uint32_t> m_kp_shader;

    void FillBuffers();

    bool m_inialized = false;
    bool m_rayDirsDirty = true;

    kp::Manager m_kp_manager;
    std::shared_ptr<kp::Algorithm> m_kp_algorithm;
    std::shared_ptr<kp::Sequence> m_kp_sequence;

    // Storage Buffers
    std::vector<std::shared_ptr<kp::Memory>>  m_kp_buffers;
    // consts and on dispatch vars
    std::vector<float> m_kp_consts;
    std::vector<PushConsts> m_kp_pushConsts;

    // Ray directions
    std::shared_ptr<kp::TensorT<float>> m_buf_raydirs;

    // Node buffers
    std::shared_ptr<kp::TensorT<float>> m_buf_nodes_BBoxes;
    std::shared_ptr<kp::TensorT<int>> m_buf_nodes_idx_tricount;

    // Triangle data
    std::shared_ptr<kp::TensorT<float>> m_buf_tris_opt;
    //std::shared_ptr<kp::TensorT<float>> m_buf_tris_normals;
    std::shared_ptr<kp::TensorT<int>> m_buf_tris_mats;
    std::shared_ptr<kp::TensorT<float>> m_buf_materials;

    // HDRI
    std::shared_ptr<kp::ImageT<float>> m_buf_hdri;

    // Textures
    std::shared_ptr<kp::TensorT<float>> m_buf_textures;
    std::shared_ptr<kp::TensorT<int>> m_buf_textureDiffuseIndices;


    // Final outbut buffer with rgba pixels
    std::shared_ptr<kp::TensorT<float>> m_buf_imgOut;
    std::shared_ptr<kp::TensorT<float>> m_buf_imgAccu;

    std::thread m_copyThread;
};
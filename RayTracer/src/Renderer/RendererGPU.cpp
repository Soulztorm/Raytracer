#include "RendererGPU.h"

#include <filesystem>

std::vector<uint32_t> RendererGPU::CompileShader(const std::string& filepath)
{
	std::string filepath_out = (filepath + ".spv");
	std::string glslCompilerArgs = "glslangValidator -V \"" + std::filesystem::absolute(filepath).string() + "\" -o \"" + std::filesystem::absolute(filepath_out).string() + "\"";
	if (system(glslCompilerArgs.c_str()))
		throw std::runtime_error("Error running glslangValidator command");
	std::ifstream fileStream(filepath_out, std::ios::binary);
	std::vector<char> buffer;
	buffer.insert(buffer.begin(), std::istreambuf_iterator<char>(fileStream), {});
	return { (uint32_t*)buffer.data(), (uint32_t*)(buffer.data() + buffer.size()) };
}


void RendererGPU::InitGPU(Scene* scene, BVH* bvh) {
	m_activeScene = scene;
	m_activeBVH = bvh;

	// Compile the shader
	m_kp_shader = CompileShader("src/Shaders/path_tracer.comp");

	m_kp_pushConsts = { { glm::mat4(), glm::mat4(), glm::vec3(0), 0 } };
}

bool RendererGPU::OnResize(uint32_t width, uint32_t height)
{
	bool didResize = Renderer::OnResize(width, height);

	if (didResize) {
		//m_kp_manager.clear();

		m_kp_imgRaw = m_kp_manager.tensor(std::vector<float>(width * height * 4));

		m_kp_params = { m_kp_imgRaw };

		m_kp_consts = { float(width), float(height) };

		m_kp_algorithm = m_kp_manager.algorithm<float, PushConsts>(
			m_kp_params,
			m_kp_shader,
			{ width * height },
			m_kp_consts,
			{ m_kp_pushConsts });

		//m_kp_manager.sequence()->eval<kp::OpSyncDevice>({ m_kp_imgRaw });
	}
	
	return didResize;
}


void RendererGPU::RenderGPU(Camera* camera)
{
	m_kp_pushConsts[0].frameIndex = m_frameindex;
	m_kp_pushConsts[0].viewMatrix = camera->GetView();
	m_kp_pushConsts[0].inverseProjectionMatrix = camera->GetInverseProjection();
	m_kp_pushConsts[0].camPos = camera->GetPosition();

	m_kp_manager.sequence()
		->record<kp::OpAlgoDispatch>(m_kp_algorithm, m_kp_pushConsts)
		->record<kp::OpSyncLocal>({ m_kp_imgRaw })
		->eval();

	m_Image->SetData(m_kp_imgRaw->data());

	m_frameindex++;
}


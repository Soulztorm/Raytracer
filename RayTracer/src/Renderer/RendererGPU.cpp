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

void RendererGPU::FillBuffers() {
	// Fill Node buffer
	std::vector<Node>* nodes = m_activeBVH->GetNodes();
	std::vector<float> bbArray(nodes->size() * 6);
	std::vector<int> itArray(nodes->size() * 2);
	
	for (int i = 0; i < nodes->size(); i ++) {
		// Copy the Bounding box
		BoundingBox bb = nodes->at(i).boundingBox;
		memcpy(&(bbArray[i * 6]), &bb, sizeof(BoundingBox));

		// Copy start index and tri count of node
		itArray[i * 2] = nodes->at(i).index;
		itArray[i * 2 + 1] = nodes->at(i).triangleCount;
	}

	m_buf_nodes_BBoxes = m_kp_manager.tensor(bbArray);
	m_buf_nodes_idx_tricount = m_kp_manager.tensorT<int>(itArray);


	// Fill triangle buffer
	std::vector<TriangleOptimized>* trisOpt = m_activeBVH->GetTrisOpt();
	std::vector<float> triOptArray(trisOpt->size() * 9);

	for (int i = 0; i < trisOpt->size(); i++) {
		TriangleOptimized t = trisOpt->at(i);
		memcpy(&(triOptArray[i * 9]), &t, 3 * sizeof(glm::vec3));
	}

	m_buf_tris_opt = m_kp_manager.tensor(triOptArray);

	// Upload buffers to GPU
	m_kp_manager.sequence()->eval<kp::OpSyncDevice>({ m_buf_nodes_BBoxes, m_buf_nodes_idx_tricount, m_buf_tris_opt });
}


void RendererGPU::InitGPU(Scene* scene, BVH* bvh) {
	m_activeScene = scene;
	m_activeBVH = bvh;

	// Compile the shader
	m_kp_shader = CompileShader("src/Shaders/path_tracer.comp");

	m_kp_pushConsts = { { glm::mat4(), glm::mat4(), glm::vec3(0), 0 } };

	FillBuffers();

}

bool RendererGPU::OnResize(uint32_t width, uint32_t height)
{
	bool didResize = Renderer::OnResize(width, height);

	if (didResize) {
		//m_kp_manager.clear();

		m_buf_imgOut = m_kp_manager.tensor(std::vector<float>(width * height * 4));
		m_kp_consts = { float(width), float(height) };

		m_kp_buffers = { m_buf_imgOut, m_buf_nodes_BBoxes, m_buf_nodes_idx_tricount, m_buf_tris_opt };

		m_kp_algorithm = m_kp_manager.algorithm<float, PushConsts>(
			m_kp_buffers,
			m_kp_shader,
			{ width * height },
			m_kp_consts,
			{ m_kp_pushConsts });

		m_inialized = true;
	}
	
	return didResize;
}

void RendererGPU::ResetFrameIndex()
{
	Renderer::ResetFrameIndex();
	if (m_inialized) {
		std::vector<float> emptyBuffer(m_buf_imgOut->size(), 0.0f);
		m_buf_imgOut->setData(emptyBuffer);
		m_kp_manager.sequence()->eval<kp::OpSyncDevice>({ m_buf_imgOut });
	}
	//m_kp_manager.sequence()->eval<kp::OpSyncLocal>({ m_buf_imgOut });
	//m_Image->SetData(m_buf_imgOut->data());
}

void RendererGPU::RenderGPU(Camera* camera)
{
	// Set the camera stuff
	m_kp_pushConsts[0].frameIndex = m_frameindex;
	m_kp_pushConsts[0].viewMatrix = camera->GetView();
	m_kp_pushConsts[0].inverseProjectionMatrix = camera->GetInverseProjection();
	m_kp_pushConsts[0].camPos = camera->GetPosition();

	// Run the shader
	m_kp_manager.sequence()
		->eval<kp::OpAlgoDispatch>(m_kp_algorithm, m_kp_pushConsts);
	
	// Every 10 frames or when moving fetch the image
	if (m_frameindex % 10 == 0 || m_frameindex <= 1) {
		m_kp_manager.sequence()->eval<kp::OpSyncLocal>({ m_buf_imgOut });
		m_Image->SetData(m_buf_imgOut->data());
	}

	m_frameindex++;
}


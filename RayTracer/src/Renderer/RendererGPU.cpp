#include "RendererGPU.h"

#include <filesystem>

std::vector<uint32_t> RendererGPU::CompileShader(const std::string& filepath)
{
	std::string filepath_out = (filepath + ".spv");
	std::string glslCompilerArgs = "glslc \"" + std::filesystem::absolute(filepath).string() + "\" --target-env=vulkan1.4 -o \"" + std::filesystem::absolute(filepath_out).string() + "\"";
	//std::string glslCompilerArgs = "glslangValidator -V \"" + std::filesystem::absolute(filepath).string() + "\" -o \"" + std::filesystem::absolute(filepath_out).string() + "\"";
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
	for (int i = 0; i < nodes->size(); i++) {
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
	std::vector<float> triOptArray(trisOpt->size() * 24);
	//std::vector<float> triNormalArray(trisOpt->size() * 9);
	std::vector<int> triMatsArray(trisOpt->size());
	for (int i = 0; i < trisOpt->size(); i++) {
		TriangleOptimized t = trisOpt->at(i);
		memcpy(&(triOptArray[i * 24]), &t, 6 * sizeof(glm::vec3) + 3 * sizeof(glm::vec2));
		//memcpy(&(triNormalArray[i * 9]), &(t.normal0), 3 * sizeof(glm::vec3));
		triMatsArray[i] = t.materialIndex;
	}
	m_buf_tris_opt = m_kp_manager.tensor(triOptArray);
	//m_buf_tris_normals = m_kp_manager.tensor(triNormalArray);
	m_buf_tris_mats = m_kp_manager.tensorT<int>(triMatsArray);


	// HDRI
	int hdri_width = m_activeScene->hdri.GetWidth();
	int hdri_height = m_activeScene->hdri.GetHeight();
	int hdri_pixelCount = hdri_width * hdri_height;
	std::vector<float> hdriArray(hdri_pixelCount * 4);
	std::vector<float> hdriCDFArray(hdri_pixelCount);
	for (int i = 0; i < hdri_pixelCount; i++) {
		hdriArray[i * 4] = m_activeScene->hdri.GetData()[i].r;
		hdriArray[i * 4 + 1] = m_activeScene->hdri.GetData()[i].g;
		hdriArray[i * 4 + 2] = m_activeScene->hdri.GetData()[i].b;
		hdriArray[i * 4 + 3] = m_activeScene->hdri.GetData()[i].a;
		hdriCDFArray[i] = m_activeScene->hdri.GetCDF()[i];
	}
	m_buf_hdri = m_kp_manager.imageT<float>(hdriArray, hdri_width, hdri_height, 4);
	m_buf_hdri_cdf = m_kp_manager.tensor(hdriCDFArray);


	// Materials
	std::vector<float> materialArray(m_activeScene->materials.size() * 12);
	std::vector<float> textureArray;
	std::vector<int> textureDiffuseIndexArray(m_activeScene->materials.size() * 3, -1);
	std::vector<int> textureSpecularIndexArray(m_activeScene->materials.size() * 3, -1);

	int currentTexPtr = 0;
	for (int i = 0; i < m_activeScene->materials.size(); i++) {
		const Material& mat = m_activeScene->materials[i];
		materialArray[i * 12] = mat.Albedo.r;
		materialArray[i * 12 + 1] = mat.Albedo.g;
		materialArray[i * 12 + 2] = mat.Albedo.b;
		materialArray[i * 12 + 3] = 1.0;
		materialArray[i * 12 + 4] = mat.Emission.r;
		materialArray[i * 12 + 5] = mat.Emission.g;
		materialArray[i * 12 + 6] = mat.Emission.b;
		materialArray[i * 12 + 7] = 1.0;
		materialArray[i * 12 + 8] = mat.Metallic;
		materialArray[i * 12 + 9] = mat.Roughness;
		materialArray[i * 12 + 10] = mat.Transparency;
		materialArray[i * 12 + 11] = mat.IOR;

		// Textures
		if (!mat.TexDiffuse.data.empty()) {
			int texSizeDiffuse = mat.TexDiffuse.width * mat.TexDiffuse.height;
			for (int d = 0; d < texSizeDiffuse; d++) {
				textureArray.push_back(mat.TexDiffuse.data[d].r);
				textureArray.push_back(mat.TexDiffuse.data[d].g);
				textureArray.push_back(mat.TexDiffuse.data[d].b);
				textureArray.push_back(mat.TexDiffuse.data[d].a);
			}
			textureDiffuseIndexArray[i * 3] = currentTexPtr;
			textureDiffuseIndexArray[i * 3 + 1] = mat.TexDiffuse.width;
			textureDiffuseIndexArray[i * 3 + 2] = mat.TexDiffuse.height;

			currentTexPtr += texSizeDiffuse;
		}
		if (!mat.TexSpecular.data.empty()) {
			int texSizeSpecular = mat.TexSpecular.width * mat.TexSpecular.height;
			for (int d = 0; d < texSizeSpecular; d++) {
				textureArray.push_back(mat.TexSpecular.data[d].r);
				textureArray.push_back(mat.TexSpecular.data[d].g);
				textureArray.push_back(mat.TexSpecular.data[d].b);
				textureArray.push_back(mat.TexSpecular.data[d].a);
			}
			textureSpecularIndexArray[i * 3] = currentTexPtr;
			textureSpecularIndexArray[i * 3 + 1] = mat.TexSpecular.width;
			textureSpecularIndexArray[i * 3 + 2] = mat.TexSpecular.height;

			currentTexPtr += texSizeSpecular;
		}
	}

	if (textureArray.empty())
		textureArray.push_back(-1.0);

	m_buf_materials = m_kp_manager.tensor(materialArray);
	m_buf_textures = m_kp_manager.tensor(textureArray);
	m_buf_textureDiffuseIndices = m_kp_manager.tensorT<int>(textureDiffuseIndexArray);
	m_buf_textureSpecularIndices = m_kp_manager.tensorT<int>(textureSpecularIndexArray);




	// Upload buffers to GPU
	m_kp_manager.sequence()->eval<kp::OpSyncDevice>({
		m_buf_nodes_BBoxes, m_buf_nodes_idx_tricount,
		//m_buf_tris_opt, m_buf_tris_normals, 
		m_buf_tris_opt,
		m_buf_tris_mats, m_buf_materials,
		m_buf_textures, 
		m_buf_textureDiffuseIndices, m_buf_textureSpecularIndices,
		m_buf_hdri, m_buf_hdri_cdf });
}


RendererGPU::~RendererGPU()
{
	m_kp_manager.destroy();
}

void RendererGPU::InitGPU(Scene* scene, BVH* bvh, Camera* cam) {
	m_activeScene = scene;
	m_activeBVH = bvh;
	m_activeCamera = cam;

	// Compile the shader
	m_kp_shader = CompileShader("src/Shaders/path_tracer.comp");

	m_kp_pushConsts = { PushConsts() };

	FillBuffers();

}

bool RendererGPU::OnResize(uint32_t width, uint32_t height)
{
	bool didResize = Renderer::OnResize(width, height);

	if (didResize) {
		m_buf_raydirs = m_kp_manager.tensor(std::vector<float>(width * height * 3));

		m_buf_imgOut = m_kp_manager.tensor(std::vector<float>(width * height * 4));
		m_buf_imgAccu = m_kp_manager.tensor(std::vector<float>(width * height * 4), kp::Memory::MemoryTypes::eStorage);

		m_kp_consts = { float(width), float(height), float(m_activeScene->hdri.GetWidth()), float(m_activeScene->hdri.GetHeight()) };

		m_kp_buffers = {
			m_buf_raydirs,
			m_buf_nodes_BBoxes, m_buf_nodes_idx_tricount,
			m_buf_tris_opt,
			//m_buf_tris_opt, m_buf_tris_normals, 
			m_buf_tris_mats, m_buf_materials,
			m_buf_textures, 
			m_buf_textureDiffuseIndices, m_buf_textureSpecularIndices,
			m_buf_hdri, m_buf_hdri_cdf,
			m_buf_imgAccu, m_buf_imgOut
		};

		m_kp_algorithm = m_kp_manager.algorithm<float, PushConsts>(
			m_kp_buffers,
			m_kp_shader,
			{ width * height },
			m_kp_consts,
			{ m_kp_pushConsts });

		m_inialized = true;

		m_rayDirsDirty = true;
	}

	return didResize;
}

void RendererGPU::ResetFrameIndex()
{
	Renderer::ResetFrameIndex();
	if (m_inialized) {
		//std::vector<float> emptyBuffer(m_buf_imgOut->size(), 0.0f);
		//m_buf_imgOut->setData(emptyBuffer);
		//m_kp_manager.sequence()->eval<kp::OpSyncDevice>({ m_buf_imgOut });
	}

	//m_kp_manager.sequence()->eval<kp::OpSyncLocal>({ m_buf_imgOut });
	//m_Image->SetData(m_buf_imgOut->data());
}

bool RendererGPU::OnCameraMoved()
{
	m_rayDirsDirty = true;
	return true;
}

void RendererGPU::RenderGPU(Camera* camera)
{
	if (m_rayDirsDirty) {
		std::vector<float> rayDirArray(m_activeCamera->GetRayDirections().size() * 3);
		for (int r = 0; r < m_activeCamera->GetRayDirections().size(); r++) {
			m_buf_raydirs->data()[r * 3] = m_activeCamera->GetRayDirections()[r].x;
			m_buf_raydirs->data()[r * 3 + 1] = m_activeCamera->GetRayDirections()[r].y;
			m_buf_raydirs->data()[r * 3 + 2] = m_activeCamera->GetRayDirections()[r].z;
		}
		m_kp_manager.sequence()->eval<kp::OpSyncDevice>({ m_buf_raydirs });

		m_rayDirsDirty = false;
	}

	// Set the camera stuff
	m_kp_pushConsts[0].camPos = glm::vec4(camera->GetPosition(), 0);
	m_kp_pushConsts[0].camRight = glm::vec4(camera->GetRight(), 0);
	m_kp_pushConsts[0].camUp = glm::vec4(camera->GetUp(), 0);
	m_kp_pushConsts[0].frameIndex = m_frameindex;
	m_kp_pushConsts[0].renderMode = m_settings.RenderMode;
	m_kp_pushConsts[0].bounces = m_settings.Bounces;
	m_kp_pushConsts[0].useACE = m_settings.UseACE_Color;
	m_kp_pushConsts[0].exposure = m_settings.Exposure;
	m_kp_pushConsts[0].dof_dist = m_settings.DoF_Distance;
	m_kp_pushConsts[0].dof_strength = m_settings.DoF_Strength;
	m_kp_pushConsts[0].skyX = m_settings.SkyX;
	m_kp_pushConsts[0].skyY = m_settings.SkyY;
	m_kp_pushConsts[0].accumulate = m_settings.Accumulate;

	// Run the shader
	m_kp_manager.sequence()
		->eval<kp::OpAlgoDispatch>(m_kp_algorithm, m_kp_pushConsts);

	// Every 10 frames or when moving fetch the image
	if (m_frameindex % 20 == 1) {
		m_kp_manager.sequence()->eval<kp::OpSyncLocal>({ m_buf_imgOut });
		m_Image->SetData(m_buf_imgOut->data());
	}


	m_frameindex++;
}


#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Random.h"
#include "Walnut/Timer.h"

#include "Renderer.h"
#include "RendererGPU.h"
#include "Camera.h"
#include "Scene.h"
#include "BVH.h"
#include "HDRI.h"

#include "imgui_internal.h"

#include <glm/gtc/type_ptr.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "Utils/tiny_obj_loader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image.h"
#include "tiny_gltf.h"

#include <filesystem>
#define fs std::filesystem


using namespace Walnut;

template <typename T, typename Total, size_t N>
class Moving_Average
{
public:
	Moving_Average& operator()(T sample)
	{
		total_ += sample;
		if (num_samples_ < N)
			samples_[num_samples_++] = sample;
		else
		{
			T& oldest = samples_[num_samples_++ % N];
			total_ -= oldest;
			oldest = sample;
		}
		return *this;
	}

	float operator()() {
		return *this;
	}

	operator float() const { return total_ / std::min(num_samples_, N); }

private:
	T samples_[N];
	size_t num_samples_{ 0 };
	Total total_{ 0 };
};

bool tryLoadTexture(Texture& texture, const std::string& texName, const std::string& texFolder, bool flipY = false) {
	if (texName.empty())
		return false;

	std::string dp = fs::absolute(texFolder + "/" + texName).string();

	int w, h, n;
	unsigned char* data = stbi_load(dp.c_str(), &w, &h, &n, 4);

	if (!data || w <= 0 || h <= 0)
		return false;

	texture.width = w;
	texture.height = h;
	texture.data.reserve(w * h);
	for (int i = 0; i < w * h; i++) {
		float r = glm::clamp(static_cast<float>(data[i * 4]) / 255.0f, 0.0f, 1.0f);
		float g = glm::clamp(static_cast<float>(data[i * 4 + 1]) / 255.0f, 0.0f, 1.0f);
		if (flipY)
			g = 1.0f - g;
		float b = glm::clamp(static_cast<float>(data[i * 4 + 2]) / 255.0f, 0.0f, 1.0f);
		float a = glm::clamp(static_cast<float>(data[i * 4 + 3]) / 255.0f, 0.0f, 1.0f);
		texture.data.emplace_back(r, g, b, a);
	}
	free(data);
}




class RaytracerLayer : public Walnut::Layer
{
public:
	RaytracerLayer() :
		m_camera(70.0f, 0.05f, 100.0f)
	{	

		LoadSettings();





		//tinygltf::Model model;
		//tinygltf::TinyGLTF loader;
		//std::string err;
		//std::string warn;

		//bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, "../Assets/sponza-scene/source/glTF/Sponza.gltf");
		////bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, "../Assets/sponza-scene/source/glTF/Sponza.gltf"); // for binary glTF(.glb)

		//model.meshes[0].primitives[0].
		//if (!warn.empty()) {
		//	printf("Warn: %s\n", warn.c_str());
		//}






		m_scene.hdri.LoadFromFile("../Assets/hdri/pretoria_gardens_4k.exr");
		//m_scene.hdri.LoadFromFile("../Assets/hdri/rosendal_plains_2_4k.exr");
		//m_scene.hdri.LoadFromFile("../Assets/hdri/rogland_clear_night_4k.exr");
		//m_scene.hdri.LoadFromFile("../Assets/hdri/qwantani_sunrise_4k.exr");

	 	// Load OBJ
		float objScale = 1.f;
		fs::path objPath;

#define SCENE 3

#if SCENE == 0
		objPath = ("../Assets/cornell-box/CornellBox-Water2.obj");
		//objPath = ("../Assets/cornell-box/CornellBox-Sphere.obj");
#elif SCENE == 1
		objPath = ("../Assets/fireplace_room/fireplace_room.obj");
#elif SCENE == 2
		objPath = ("../Assets/bistro/bistro.obj");
		objScale = 1.f;
#elif SCENE == 3
		objPath = ("../Assets/sponza/sponza.obj");
		objScale = 0.1f;
#elif SCENE == 4
		objPath = ("../Assets/redspheres.obj");
#endif

		//m_camera.SetSpeed(1.0f * objScale);

		tinyobj::ObjReader Reader;
		tinyobj::ObjReaderConfig config;
		config.triangulate = true;


		if (Reader.ParseFromFile(objPath.string(), config)) 
		{
			// Add default material
			m_scene.materials.emplace_back(Material());

			auto& attrib = Reader.GetAttrib();
			auto& shapes = Reader.GetShapes();
			auto& materials = Reader.GetMaterials();

			for each (const auto & _mat in materials)
			{
				Material& mat = m_scene.materials.emplace_back();
				mat.Name = _mat.name;
				mat.Albedo = glm::vec3(_mat.diffuse[0], _mat.diffuse[1], _mat.diffuse[2]);
				mat.Emission = glm::vec3(_mat.emission[0], _mat.emission[1], _mat.emission[2]);
				mat.Metallic = glm::clamp(_mat.metallic, 0.0f, 1.0f);
				mat.Specular = glm::vec3(_mat.specular[0], _mat.specular[1], _mat.specular[2]);
				mat.Roughness = glm::clamp((1.0f - (_mat.shininess / 1000.0f)) , 0.0f, 1.0f);
				mat.IOR = _mat.ior;
				
				// Diffuse Texture
				tryLoadTexture(mat.TexDiffuse, _mat.diffuse_texname, objPath.parent_path().string());
				// Normal Texture
				tryLoadTexture(mat.TexNormal, _mat.bump_texname, objPath.parent_path().string(), true);
				// Specular Texture
				tryLoadTexture(mat.TexSpecular, _mat.specular_texname, objPath.parent_path().string());
			}

			
			// Loop over shapes
			for (size_t s = 0; s < shapes.size(); s++) {
				// Loop over faces(polygon)
				size_t index_offset = 0;
				for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
					TriangleOBJ tri;
					glm::vec3 avg_centroid{ 0.0f };

					int MatID = shapes[s].mesh.material_ids[f];

					// Loop over vertices in the face.
					for (size_t v = 0; v < 3; v++) {
						// access to vertex
						tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
						tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
						tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
						tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

						glm::vec3 vertexPosition = glm::vec3(vx, vy, vz) * objScale;
						tri.Vertices.push_back(vertexPosition);
						avg_centroid += vertexPosition;


						glm::vec3 normal = glm::vec3(0.0, 1.0, 0.0);
						// Check if `normal_index` is zero or positive. negative = no normal data
						if (idx.normal_index >= 0) {
							tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
							tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
							tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

							normal = glm::normalize(glm::vec3(nx, ny, nz));
						}
						tri.Normals.push_back(normal);

						glm::vec2 tcoords(0);
						// Check if `texcoord_index` is zero or positive. negative = no texcoord data
						if (idx.texcoord_index >= 0) {
							tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
							tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
							tcoords = glm::vec2(tx, ty);
						}
						tri.TCoords.push_back(tcoords);

						// Optional: vertex colors
						// tinyobj::real_t red   = attrib.colors[3*size_t(idx.vertex_index)+0];
						// tinyobj::real_t green = attrib.colors[3*size_t(idx.vertex_index)+1];
						// tinyobj::real_t blue  = attrib.colors[3*size_t(idx.vertex_index)+2];
					}

					// Compute tangents, needs tcoords
					if (tri.Normals.size() >= 3 && tri.TCoords.size() >= 3) {
						tri.Tangents.resize(3);

						glm::vec3 e1 = tri.Vertices[1] - tri.Vertices[0];
						glm::vec3 e2 = tri.Vertices[2] - tri.Vertices[0];

						glm::vec2 deltaUV1 = tri.TCoords[1] - tri.TCoords[0];
						glm::vec2 deltaUV2 = tri.TCoords[2] - tri.TCoords[0];

						float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
						glm::vec3 faceTangent = f * (deltaUV2.y * e1 - deltaUV1.y * e2);
						glm::vec3 faceBitangent = f * (-deltaUV2.x * e1 + deltaUV1.x * e2);

						glm::vec3 t0 = glm::normalize(faceTangent - tri.Normals[0] * glm::dot(tri.Normals[0], faceTangent));
						glm::vec3 b0 = glm::cross(tri.Normals[0], t0);
						float h0 = (glm::dot(b0, faceBitangent) < 0.0) ? -1.0 : 1.0;

						glm::vec3 t1 = glm::normalize(faceTangent - tri.Normals[1] * glm::dot(tri.Normals[1], faceTangent));
						glm::vec3 b1 = glm::cross(tri.Normals[1], t1);
						float h1 = (glm::dot(b1, faceBitangent) < 0.0) ? -1.0 : 1.0;

						glm::vec3 t2 = glm::normalize(faceTangent - tri.Normals[2] * glm::dot(tri.Normals[2], faceTangent));
						glm::vec3 b2 = glm::cross(tri.Normals[2], t2);
						float h2 = (glm::dot(b2, faceBitangent) < 0.0) ? -1.0 : 1.0;

						tri.Tangents[0] = glm::vec4(t0, h0);
						tri.Tangents[1] = glm::vec4(t1, h1);
						tri.Tangents[2] = glm::vec4(t2, h2);
					}


					tri.Center = avg_centroid / 3.0f;

					tri.MaterialIndex = std::max(MatID + 1, 0);
					m_scene.triangles.push_back(tri);

					index_offset += 3;
				}
			}


			#define ADDLIGHT 0
			#if ADDLIGHT
				Material& lightmat = m_scene.materials.emplace_back();
				lightmat.Emission = glm::vec3(10.0f, 8.7f, 7.0f);
				lightmat.Name = "PRAISE THE SUN";
				TriangleOBJ skyLight;
				skyLight.Vertices.push_back(glm::vec3(10000.0f, 10000.0f, 10000.0f));
				skyLight.Vertices.push_back(glm::vec3(-10000.0f, 10000.0f, 10000.0f));
				skyLight.Vertices.push_back(glm::vec3(10000.0f, 10000.0f, -10000.0f));

				skyLight.Normals.push_back(glm::vec3(0,-1, 0));
				skyLight.Normals.push_back(glm::vec3(0,-1, 0));
				skyLight.Normals.push_back(glm::vec3(0,-1, 0));

				skyLight.Center = (skyLight.Vertices[0] + skyLight.Vertices[1] + skyLight.Vertices[2]) / 3.0f;

				skyLight.MaterialIndex = m_scene.materials.size() - 1;

				m_scene.triangles.push_back(skyLight);
				skyLight.Vertices[0] = glm::vec3(-10000.0f, 10000.0f, -10000.0f);
				m_scene.triangles.push_back(skyLight);
			#endif

			
			m_bvh = std::make_shared<BVH>(m_scene);
			std::cout << "NodeCount: " << m_bvh->GetNodeCount();

			m_renderer.InitGPU(&m_scene, m_bvh.get(), &m_camera);
		}		
	}

	~RaytracerLayer() {
		ImGui::SaveIniSettingsToDisk("imgui.ini");
	}

	virtual void OnUpdate(float ts) override
	{
		if (m_camera.OnUpdate(ts)) {
			m_renderer.OnCameraMoved();
			m_renderer.ResetFrameIndex();
		}
	}

	virtual void OnUIRender() override
	{
		Renderer::Settings oldSettings = m_renderer.GetSettings();
		// Settings
		ImGui::Begin("Settings");
		ImGui::Text("Last render: %.3fms | %i", m_lastRenderTimes(), m_renderer.GetFrameIndex());
		ImGui::Checkbox("GPU", &m_renderer.GetSettings().UseGPU);
		ImGui::Checkbox("Render", &m_renderer.GetSettings().Render);
		ImGui::SliderInt("Rendermode", (int*)&m_renderer.GetSettings().RenderMode, 0, 1);
		ImGui::Checkbox("Accumulate", &m_renderer.GetSettings().Accumulate);
		ImGui::DragFloat("Exposure", &m_renderer.GetSettings().Exposure, 0.01f, 0.0f, 10000.0f);
		ImGui::Checkbox("Use ACE Color", &m_renderer.GetSettings().UseACE_Color);
		ImGui::DragInt("# Bounces", (int*)&m_renderer.GetSettings().Bounces, 0.05f, 0);

		ImGui::DragFloat("DoF Strength", &m_renderer.GetSettings().DoF_Strength, 0.5f, 0.0f, 1000.0f);
		ImGui::DragFloat("DoF Distance", &m_renderer.GetSettings().DoF_Distance, 0.05f, 0.0f, 10000.0f);

		ImGui::DragFloat("Sky X", &m_renderer.GetSettings().SkyX, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Sky Y", &m_renderer.GetSettings().SkyY, 0.01f, 0.0f, 1.0f);
		if (ImGui::Button("Reset Camera"))
			m_camera.Reset();

		if (m_renderer.GetSettings().RenderMode != oldSettings.RenderMode || 
			m_renderer.GetSettings().UseGPU != oldSettings.UseGPU ||
			m_renderer.GetSettings().DoF_Strength != oldSettings.DoF_Strength ||
			m_renderer.GetSettings().DoF_Distance != oldSettings.DoF_Distance ||
			m_renderer.GetSettings().SkyY != oldSettings.SkyY ||
			m_renderer.GetSettings().SkyX != oldSettings.SkyX)
			m_renderer.ResetFrameIndex();

		//ImGui::SliderFloat3("Light Position:", glm::value_ptr(m_scene.lightPosition), -10.0f, 10.0f, "%.2f");
		//ImGui::SliderFloat("Light Power:", &m_scene.lightPower, -1.0f, 2.0f, "%.2f");
		ImGui::End();

		// Materials
		ImGui::Begin("Materials");

		for (size_t i = 0; i < m_scene.materials.size(); i++)
		{
			Material& mat = m_scene.materials[i];
			std::string materialString = !mat.Name.empty() ? mat.Name : "Material %i";

			ImGui::PushID((int)i);
			if (ImGui::CollapsingHeader(materialString.c_str())) {
				ImGui::ColorEdit3("Albedo", glm::value_ptr(mat.Albedo));
				ImGui::ColorEdit3("Specular", glm::value_ptr(mat.Specular));
				ImGui::DragFloat3("Emission", glm::value_ptr(mat.Emission), 0.1f, 0.0f, 200.0f);
				ImGui::DragFloat("Metallic", &mat.Metallic, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Roughness", &mat.Roughness, 0.01f, 0.0f, 1.0f);
			}
			ImGui::PopID();
			ImGui::Separator();


			//if (!mat.Name.empty())
			//	ImGui::Text(mat.Name.c_str());
			//else
			//	ImGui::Text("Material %i", i);


		}

		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");

		m_viewportWidth = (uint32_t)ImGui::GetContentRegionAvail().x;
		m_viewportHeight = (uint32_t)ImGui::GetContentRegionAvail().y;

		if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Middle)) {
			auto pos = glm::vec2(ImGui::GetMousePos().x - ImGui::GetWindowPos().x, m_viewportHeight - (ImGui::GetMousePos().y - ImGui::GetWindowPos().y) - 1);
			Ray ray;
			ray.Origin = m_camera.GetPosition();
			ray.Direction = m_camera.GetRayDirections()[pos.y * m_viewportWidth + pos.x];
			ray.DirectionInverse = glm::vec3(1.0 / ray.Direction.x, 1.0 / ray.Direction.y, 1.0 / ray.Direction.z);

			auto hitinfo = m_bvh->IntersectRay(&ray);
			if (hitinfo.materialIndex > 0)
			{
				m_renderer.GetSettings().DoF_Distance = glm::length(hitinfo.position - ray.Origin);
				m_renderer.ResetFrameIndex();
			}
		}

		auto img = m_renderer.GetImage();
		if (img) {
			ImGui::Image((ImTextureID)img->GetDescriptorSet(), { (float)img->GetWidth(), (float)img->GetHeight() }, ImVec2(0, 1), ImVec2(1, 0));
		}

		ImGui::End();
		ImGui::PopStyleVar();

		// Render every frame
		if (m_renderer.GetSettings().Render)
			Render();
	}

	void Render() {
		Timer timer;

		// resize if needed
		m_camera.OnResize(m_viewportWidth, m_viewportHeight);
		m_renderer.OnResize(m_viewportWidth, m_viewportHeight);

		// render
		if (m_renderer.GetSettings().UseGPU)
			m_renderer.RenderGPU(&m_camera);
		else
			m_renderer.Render(&m_scene, m_bvh.get(), &m_camera);

		m_lastRenderTimes(timer.ElapsedMillis());	
	}

	void LoadSettings() {
		ImGuiSettingsHandler ini_handler;
		ini_handler.TypeName = "UserData";
		ini_handler.TypeHash = ImHashStr("UserData");
		ini_handler.ReadOpenFn = UserData_ReadOpen;
		ini_handler.ReadLineFn = UserData_ReadLine;
		ini_handler.WriteAllFn = UserData_WriteAll;
		ini_handler.UserData = this;
		ImGui::AddSettingsHandler(&ini_handler);

		ImGui::LoadIniSettingsFromDisk("imgui.ini");
	}

	static void* UserData_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
	{
		return (void*)"Camera";
	}

	static void UserData_ReadLine(ImGuiContext*, ImGuiSettingsHandler* handler, void* entry, const char* line)
	{
		RaytracerLayer* rt = (RaytracerLayer*)handler->UserData;
		
		glm::vec3 position = rt->m_camera.GetPosition();
		glm::vec3 direction = rt->m_camera.GetDirection();
		
		std::string lineStr(line);

		if (lineStr._Starts_with("pos=")) {
			float posArr[3];
			std::string posCoordStr = lineStr.substr(lineStr.find_last_of('=')+1);

			std::string number;
			size_t i = 0;
			std::stringstream ss(posCoordStr);
			// Parse CSV values into the array
			while (std::getline(ss, number, ',') && i < 3) {
				posArr[i++] = std::stof(number);
			}
			position = glm::vec3(posArr[0], posArr[1], posArr[2]);
		}

		else if (lineStr._Starts_with("dir=")) {
			float dirArr[3];
			std::string dirCoordStr = lineStr.substr(lineStr.find_last_of('=') + 1);

			std::string number;
			size_t i = 0;
			std::stringstream ss(dirCoordStr);
			// Parse CSV values into the array
			while (std::getline(ss, number, ',') && i < 3) {
				dirArr[i++] = std::stof(number);
			}
			direction = glm::vec3(dirArr[0], dirArr[1], dirArr[2]);
		}
		else if (lineStr._Starts_with("skyX="))
			rt->m_renderer.GetSettings().SkyX = std::stof(lineStr.substr(lineStr.find_last_of('=') + 1));
		else if (lineStr._Starts_with("skyY="))
			rt->m_renderer.GetSettings().SkyY = std::stof(lineStr.substr(lineStr.find_last_of('=') + 1));
		else if (lineStr._Starts_with("exposure="))
			rt->m_renderer.GetSettings().Exposure = std::stof(lineStr.substr(lineStr.find_last_of('=') + 1));
		else if (lineStr._Starts_with("bounces="))
			rt->m_renderer.GetSettings().Bounces = std::stoi(lineStr.substr(lineStr.find_last_of('=') + 1));
		else if (lineStr._Starts_with("dofstrength="))
			rt->m_renderer.GetSettings().DoF_Strength = std::stof(lineStr.substr(lineStr.find_last_of('=') + 1));
		else if (lineStr._Starts_with("dofdistance="))
			rt->m_renderer.GetSettings().DoF_Distance = std::stof(lineStr.substr(lineStr.find_last_of('=') + 1));

		if (direction == glm::vec3(0.0f))
			direction = glm::vec3(1, 0, 0);

		if (rt->m_renderer.GetSettings().Bounces == 0)
			rt->m_renderer.GetSettings().Bounces = 6;

		if (rt->m_renderer.GetSettings().Exposure == 0.0f)
			rt->m_renderer.GetSettings().Exposure = 1.0f;

		rt->m_camera.SetPositionDirection(position, direction);
	}

	static void UserData_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
	{
		RaytracerLayer* rt = (RaytracerLayer*)handler->UserData;

		const glm::vec3& pos = rt->m_camera.GetPosition();
		const glm::vec3& dir = rt->m_camera.GetDirection();

		buf->appendf("[%s][%s]\n", "UserData", "Camera");
		buf->appendf("pos=%f,%f,%f\n", pos.x, pos.y, pos.z);
		buf->appendf("dir=%f,%f,%f\n", dir.x, dir.y, dir.z);
		buf->append("\n");
		buf->appendf("[%s][%s]\n", "UserData", "Settings");
		buf->appendf("skyX=%f\n", rt->m_renderer.GetSettings().SkyX);
		buf->appendf("skyY=%f\n", rt->m_renderer.GetSettings().SkyY);
		buf->appendf("exposure=%f\n", rt->m_renderer.GetSettings().Exposure);
		buf->appendf("bounces=%i\n", rt->m_renderer.GetSettings().Bounces);
		buf->appendf("dofstrength=%f\n", rt->m_renderer.GetSettings().DoF_Strength);
		buf->appendf("dofdistance=%f\n", rt->m_renderer.GetSettings().DoF_Distance);

	}


private:
	Camera m_camera;
	Scene m_scene;
	RendererGPU m_renderer;

	std::shared_ptr<BVH> m_bvh;

	uint32_t m_viewportWidth = 0, m_viewportHeight = 0;

	// Gui vars
	Moving_Average<float, float, 1> m_lastRenderTimes;
};



Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Raytracer go BRRRRRRR";
	spec.Width = 900;
	spec.Height = 600;

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<RaytracerLayer>();
	return app;
}
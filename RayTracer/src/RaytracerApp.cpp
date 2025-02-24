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

#include "stb_image.h"

#include <filesystem>
#define fs std::filesystem

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

using namespace Walnut;

class RaytracerLayer : public Walnut::Layer
{
public:
	RaytracerLayer() :
		m_camera(70.0f, 0.05f, 100.0f)
	{	

		LoadSettings();

		//m_scene.hdri.LoadFromFile("../Assets/hdri/pretoria_gardens_4k.exr");
		m_scene.hdri.LoadFromFile("../Assets/hdri/rosendal_plains_2_4k.exr");
		//m_scene.hdri.LoadFromFile("../Assets/hdri/rogland_clear_night_4k.exr");
		//m_scene.hdri.LoadFromFile("../Assets/hdri/qwantani_sunrise_4k.exr");

	 	// Load OBJ
#if 1
		fs::path objPath("../Assets/sponza/sponza.obj");
		float objScale = 0.01f;
#else

		fs::path objPath("../Assets/fireplace_room/fireplace_room.obj");
		float objScale = 1.f;
#endif
		//fs::path objPath("../Assets/cornell-box/CornellBox-Water2.obj");

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

				mat.Albedo = 
					glm::vec3(_mat.diffuse[0], _mat.diffuse[1], _mat.diffuse[2]);
					//glm::vec3(_mat.specular[0], _mat.specular[1], _mat.specular[2]));
				mat.Emission = glm::vec3(_mat.emission[0], _mat.emission[1], _mat.emission[2]);
				mat.Metallic = _mat.metallic;
				mat.Roughness = (1000.0f - _mat.shininess) / 1000.0f;
				mat.IOR = _mat.ior;

				if (mat.Emission.r == 0 && mat.Emission.g == 0 && mat.Emission.b == 0 && _mat.illum == 3)
					mat.Transparency = 1.0f - _mat.transmittance[0];

				mat.Name = _mat.name;

				if (_mat.name == "Material__25")
					mat.Metallic = 1.0;


				if (!_mat.diffuse_texname.empty()) {
					std::string dp = fs::absolute(objPath.parent_path().string() + "/" + _mat.diffuse_texname).string();

					int w, h, n;
					unsigned char* data = stbi_load(dp.c_str(), &w, &h, &n, 4);

					if (data) {
						mat.TexDiffuse.width = w;
						mat.TexDiffuse.height = h;
						for (int i = 0; i < w * h; i++) {
							float r = (static_cast<float>(data[i * 4]) / 255.0f);
							float g = (static_cast<float>(data[i * 4 + 1]) / 255.0f);
							float b = (static_cast<float>(data[i * 4 + 2]) / 255.0f);
							float a = (static_cast<float>(data[i * 4 + 3]) / 255.0f);
							mat.TexDiffuse.data.emplace_back(r, g, b, a);
						}
					}
					free(data);
				}
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

		ImGui::DragFloat("DoF Strength", &m_renderer.GetSettings().DoF_Strength, 0.0001f, 0.0f, 10.0f);
		ImGui::DragFloat("DoF Distance", &m_renderer.GetSettings().DoF_Distance, 0.05f, 0.0f, 10000.0f);

		ImGui::DragFloat("Sky X", &m_renderer.GetSettings().SkyX, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Sky Y", &m_renderer.GetSettings().SkyY, 0.01f, 0.0f, 1.0f);

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

			ImGui::PushID((int)i);
			if (!mat.Name.empty())
				ImGui::Text(mat.Name.c_str());
			else
				ImGui::Text("Material %i", i);

			ImGui::ColorEdit3("Albedo", glm::value_ptr(mat.Albedo));
			ImGui::DragFloat3("Emission", glm::value_ptr(mat.Emission), 0.1f, 0.0f, 200.0f);
			ImGui::DragFloat("Roughness", &mat.Roughness, 0.01f, 0.0f, 1.0f);
			ImGui::PopID();
			ImGui::Separator();
			ImGui::Separator();
		}

		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");

		m_viewportWidth = (uint32_t)ImGui::GetContentRegionAvail().x;
		m_viewportHeight = (uint32_t)ImGui::GetContentRegionAvail().y;

		if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Middle)) {
			auto pos = glm::vec2(ImGui::GetMousePos().x - ImGui::GetWindowPos().x, ImGui::GetMousePos().y - ImGui::GetWindowPos().y);
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
	spec.Width = 1280;
	spec.Height = 720;

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<RaytracerLayer>();
	return app;
}
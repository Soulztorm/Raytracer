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

#include <glm/gtc/type_ptr.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "Utils/tiny_obj_loader.h"

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
		// Load OBJ
		tinyobj::ObjReader Reader;
		tinyobj::ObjReaderConfig config;
		config.triangulate = true;

		m_scene.hdri.LoadFromFile("../Assets/hdri/pretoria_gardens_4k.exr");

		if (Reader.ParseFromFile("../Assets/sponza-scene/sponza.obj", config)) {
		//if (Reader.ParseFromFile("../Assets/cornell-box/CornellBox-Water.obj", config)) {
			auto& attrib = Reader.GetAttrib();
			auto& shapes = Reader.GetShapes();
			auto& materials = Reader.GetMaterials();


			for each (const auto & _mat in materials)
			{
				Material& mat = m_scene.materials.emplace_back();

				mat.Albedo = glm::max(
					glm::vec3(_mat.diffuse[0], _mat.diffuse[1], _mat.diffuse[2]),
					glm::vec3(_mat.specular[0], _mat.specular[1], _mat.specular[2]));
				mat.Emission = 2.0f * glm::vec3(_mat.emission[0], _mat.emission[1], _mat.emission[2]);
				mat.Roughness = (1024.0f - _mat.shininess) / 1024.0f;
				mat.IOR = _mat.ior;
				if (_mat.name == "water")
					mat.Transparency = 1.0f;
				mat.Name = _mat.name;
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

						glm::vec3 vertexPosition = glm::vec3(vx, vy, vz);
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

						// Check if `texcoord_index` is zero or positive. negative = no texcoord data
						if (idx.texcoord_index >= 0) {
							tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
							tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
						}

						// Optional: vertex colors
						// tinyobj::real_t red   = attrib.colors[3*size_t(idx.vertex_index)+0];
						// tinyobj::real_t green = attrib.colors[3*size_t(idx.vertex_index)+1];
						// tinyobj::real_t blue  = attrib.colors[3*size_t(idx.vertex_index)+2];
					}

					tri.Center = avg_centroid / 3.0f;

					tri.MaterialIndex = std::max(MatID, 0);
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

	virtual void OnUpdate(float ts) override
	{
		if (m_camera.OnUpdate(ts)) {
			m_renderer.OnCameraMoved();
			m_renderer.ResetFrameIndex();
		}
	}

	virtual void OnUIRender() override
	{
		int currentRenderMode = m_renderer.GetSettings().RenderMode;
		bool usingGPU = m_renderer.GetSettings().UseGPU;

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

		ImGui::DragFloat("DoF Strength", &m_renderer.GetSettings().DoF_Strength, 0.001f, 0.0f, 0.1f);
		ImGui::DragFloat("DoF Distance", &m_renderer.GetSettings().DoF_Distance, 0.01f, 0.0f, 10000.0f);

		if (m_renderer.GetSettings().RenderMode != currentRenderMode || m_renderer.GetSettings().UseGPU != usingGPU)
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

		auto img = m_renderer.GetImage();
		if (img) {
			ImGui::Image(img->GetDescriptorSet(), { (float)img->GetWidth(), (float)img->GetHeight() }, ImVec2(0, 1), ImVec2(1, 0));
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


private:
	Camera m_camera;
	Scene m_scene;
	RendererGPU m_renderer;
	// m_vulkanRenderer;

	std::shared_ptr<BVH> m_bvh;

	uint32_t m_viewportWidth = 0, m_viewportHeight = 0;

	// Gui vars
	Moving_Average<float, float, 10> m_lastRenderTimes;
};



Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Raytracer go BRRRRRRR";
	spec.Width = 900;
	spec.Height = 480;

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<RaytracerLayer>();
	return app;
}
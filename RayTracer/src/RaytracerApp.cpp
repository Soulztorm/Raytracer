#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Random.h"
#include "Walnut/Timer.h"

#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"
//#include "KDtree.h"

#include <glm/gtc/type_ptr.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

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

		if (Reader.ParseFromFile("../Assets/cornell-box/CornellBox-Water.obj", config)) {
			auto& attrib = Reader.GetAttrib();
			auto& shapes = Reader.GetShapes();
			auto& materials = Reader.GetMaterials();


			for each (const auto & _mat in materials)
			{
				Material& mat = m_scene.materials.emplace_back();

				mat.Albedo = glm::max(
					glm::vec3(_mat.diffuse[0], _mat.diffuse[1], _mat.diffuse[2]),
					glm::vec3(_mat.specular[0], _mat.specular[1], _mat.specular[2]));
				mat.Emission = 5.0f * glm::vec3(_mat.emission[0], _mat.emission[1], _mat.emission[2]);
				mat.Roughness = (1024.0f - _mat.shininess) / 1024.0f;
				mat.IOR = _mat.ior;
				if (_mat.name == "water")
					mat.Transparency = 1.0f;
				mat.Name = _mat.name;
			}


			std::vector<glm::vec3> vertices;
			std::vector<glm::uvec3> triindexes;

			uint32_t currentVertexIndex = 0;

			// Loop over shapes
			for (size_t s = 0; s < shapes.size(); s++) {
				// Loop over faces(polygon)
				size_t index_offset = 0;
				for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
					size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
					Triangle tri;
					glm::vec3 avg_normal{ 0.0f };
					glm::vec3 avg_centroid{ 0.0f };

					int MatID = shapes[s].mesh.material_ids[f];

					// Loop over vertices in the face.
					for (size_t v = 0; v < fv; v++) {
						// access to vertex
						tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
						tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
						tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
						tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

						float scale = 1.0f;
						glm::vec3 vertexPosition = glm::vec3(vx, vy, vz) * scale;
						vertices.push_back(vertexPosition);
						tri.Vertices.push_back(vertexPosition);
						avg_centroid += vertexPosition;

						glm::vec3 normal = glm::vec3(0.0, 1.0, 0.0);

						// Check if `normal_index` is zero or positive. negative = no normal data
						if (idx.normal_index >= 0) {
							tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
							tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
							tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

							normal = glm::normalize(glm::vec3(nx, ny, nz));
							tri.Normals.push_back(normal);

							avg_normal += glm::vec3(nx, ny, nz);
						}

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

					glm::uvec3 triindex(currentVertexIndex, currentVertexIndex + 1, currentVertexIndex + 2);
					currentVertexIndex += 3;
					triindexes.push_back(triindex);

					//tri.Normal = glm::normalize(avg_normal);
					tri.Centroid = avg_centroid / (float)fv;

					tri.MaterialIndex = std::max(MatID, 0);
					m_scene.triangles.push_back(tri);


					index_offset += fv;

					// per-face material
					//shapes[s].mesh.material_ids[f];
				}
			}


			m_scene.kd_tree = std::make_shared<KDTreeCPU>((int)triindexes.size(), &triindexes[0], (int)vertices.size(), &vertices[0]);
		}
		uint32_t matOffset = (uint32_t) m_scene.materials.size();



		// Materials
		// Floor
		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(0.8, 0.8, 0.8);
			mat.Roughness = 1.0f;
			mat.Name = "Sphere Floor";
		}
		// Left wall
		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(0.35, 1.0, 0.17);
			mat.Roughness = 1.0f;
		}
		// Right wall
		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(1.0, 0.0, 0.0);
			mat.Roughness = 1.0f;
		}


		// Sphere mats
		// Right
		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = { 0.0, 0.5, 1.0 };
			mat.Roughness = 0.9f;
		}
		// Left
		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(1.0, 0.8, 0.0);
			mat.Roughness = 0.0f;
			mat.Transparency = 1.0f;
			mat.IOR = 1.1f;
		}


		// Light
		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(1.0, 1.0, 1.0);
			mat.Emission = glm::vec3{ 20.0f };
		}

		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(1.0, 1.0, 1.0);
			mat.Emission = glm::vec3{ 0.0f };
			mat.Roughness = 0.0f;
		}



		// Floor
		{
			Sphere sphere;
			sphere.Radius = 1000.0;
			sphere.MaterialIndex = 0 + matOffset;

			sphere.Position = glm::vec3(0.0, 1002.0, 0.0);
			m_scene.spheres.push_back(sphere);

			sphere.Position = glm::vec3(0.0, -1000.0, 0.0);
			m_scene.spheres.push_back(sphere);

			sphere.Position = glm::vec3(0.0, 0.0, -1002.0);
			m_scene.spheres.push_back(sphere);

			// Front wall
			if (0) {
				sphere.Position = glm::vec3(0.0, 0.0, 1001.0);
				m_scene.spheres.push_back(sphere);
			}

			sphere.Position = glm::vec3(-1002.0, 0.0, 0.0);
			sphere.MaterialIndex = 1 + matOffset;
			m_scene.spheres.push_back(sphere);

			sphere.Position = glm::vec3(1002.0, 0.0, 0.0);
			sphere.MaterialIndex = 2 + matOffset;
			m_scene.spheres.push_back(sphere);

		}


		{
			Sphere& sphere = m_scene.spheres.emplace_back();
			sphere.Radius = 0.3f;
			sphere.MaterialIndex = 3 + matOffset;

			sphere.Position = glm::vec3(0.5, 0.3, -0.3);
		}

		{
			Sphere& sphere = m_scene.spheres.emplace_back();
			sphere.Radius = 0.3f;
			sphere.MaterialIndex = 4 + matOffset;

			sphere.Position = glm::vec3(-0.5, 0.3, -0.1);
		}

		{
			Sphere& sphere = m_scene.spheres.emplace_back();
			sphere.Radius = 0.5f;
			sphere.MaterialIndex = 6 + matOffset;

			sphere.Position = glm::vec3(0.0, 0.5, -1.5);
		}

		// Light
		{
			Sphere& sphere = m_scene.spheres.emplace_back();
			sphere.Radius = 1.0;
			sphere.MaterialIndex = 5 + matOffset;

			sphere.Position = glm::vec3(0.0, 2.9, 0.0);
		}

		
	}

	virtual void OnUpdate(float ts) override
	{
		if (m_camera.OnUpdate(ts))
			m_renderer.ResetFrameIndex();
	}

	virtual void OnUIRender() override
	{
		// Settings
		ImGui::Begin("Settings");
		ImGui::Text("Last render: %.3fms | %i", m_lastRenderTime, m_renderer.GetFrameIndex());
		ImGui::Checkbox("Render", &m_renderer.GetSettings().Render);
		ImGui::Checkbox("Accumulate", &m_renderer.GetSettings().Accumulate);
		ImGui::Checkbox("Use Sphere Scene", &m_renderer.GetSettings().UseSphereScene);
		ImGui::Checkbox("Use ACE Color", &m_renderer.GetSettings().UseACE_Color);
		ImGui::Checkbox("AA", &m_renderer.GetSettings().AntiAliasing);
		ImGui::DragInt("# Bounces", (int*)&m_renderer.GetSettings().Bounces, 0.05f, 0);


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

		// Walls
		ImGui::Begin("Walls");
		for (size_t i = 0; i < m_scene.spheres.size(); i++)
		{
			ImGui::PushID((int)i);
			ImGui::SliderInt("Material Index", (int*)&m_scene.spheres[i].MaterialIndex, 0, (int)m_scene.materials.size() - 1);
			ImGui::PopID();
			ImGui::Separator();
		}


		ImGui::End();

		// Spheres
		ImGui::Begin("Spheres");
		for (size_t i = 0; i < m_scene.spheres.size(); i++)
		{
			ImGui::PushID((int)i);
			ImGui::DragFloat3("Position", glm::value_ptr(m_scene.spheres[i].Position), 0.1f);
			ImGui::DragFloat("Radius", &m_scene.spheres[i].Radius, 0.01f, 0.1f, 5.0f);
			ImGui::SliderInt("Material Index", (int*)&m_scene.spheres[i].MaterialIndex, 0, (int)m_scene.materials.size() - 1);
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


		//ImGui::ShowDemoWindow();

		// Render every frame
		if (m_renderer.GetSettings().Render)
			Render();
	}

	void Render() {
		Timer timer;

		// resize if needed
		m_renderer.OnResize(m_viewportWidth, m_viewportHeight);
		m_camera.OnResize(m_viewportWidth, m_viewportHeight);

		// render
		m_renderer.Render(m_scene, m_camera);

		m_lastRenderTime = timer.ElapsedMillis();
	}


private:
	Camera m_camera;
	Scene m_scene;
	Renderer m_renderer;


	uint32_t m_viewportWidth = 0, m_viewportHeight = 0;

	// Gui vars
	float m_lastRenderTime = 0.0f;
};










Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Raytracer go BRRRRRRR";
	spec.Width = 1280;
	spec.Height = 720;

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<RaytracerLayer>();
	//app->SetMenubarCallback([app]()
	//{
	//	if (ImGui::BeginMenu("File"))
	//	{
	//		if (ImGui::MenuItem("Exit"))
	//		{
	//			app->Close();
	//		}
	//		ImGui::EndMenu();
	//	}
	//});
	return app;
}
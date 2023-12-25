#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Random.h"
#include "Walnut/Timer.h"

#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"

#include <glm/gtc/type_ptr.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

using namespace Walnut;

class RaytracerLayer : public Walnut::Layer
{
public:
	RaytracerLayer() :
		m_camera(80.0f, 0.1f, 100.0f)
	{
		// Materials
		// Floor
		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(0.8, 0.8, 0.8);
			mat.Roughness = 1.0f;
			mat.Metallic = 0.0f;
		}
		// Left wall
		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(0.35, 1.0, 0.17);
			mat.Roughness = 1.0f;
			mat.Metallic = 1.0f;
		}
		// Right wall
		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(1.0, 0.0, 0.0);
			mat.Roughness = 1.0f;
			mat.Metallic = 1.0f;
		}


		// Sphere mats
		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = { 0.0, 0.5, 1.0 };
			mat.Roughness = 0.9f;
			mat.Metallic = 0.0f;
		}

		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(1.0, 0.8, 0.0);
			mat.Roughness = 0.9f;
			mat.Metallic = 1.0f;
		}


		// Light
		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(1.0, 1.0, 1.0);
			mat.Emission = 20.0f;
		}

		{
			Material& mat = m_scene.materials.emplace_back();
			mat.Albedo = glm::vec3(1.0, 1.0, 1.0);
			mat.Emission = 0.0f;
			mat.Roughness = 0.0f;
		}


		if (0) {
			// Floor
			{
				Sphere sphere; 
				sphere.Radius = 1000.0;
				sphere.MaterialIndex = 0;

				sphere.Position = glm::vec3(0.0, -1000.0, 0.0);
				m_scene.spheres.push_back(sphere);

				sphere.Position = glm::vec3(0.0, 1010.0, 0.0);
				m_scene.spheres.push_back(sphere);

				sphere.Position = glm::vec3(0.0, 0.0, -1005.0);
				m_scene.spheres.push_back(sphere);

				// Front wall
				if (1) {
					sphere.Position = glm::vec3(0.0, 0.0, 1005.0);
					m_scene.spheres.push_back(sphere);
				}

				sphere.Position = glm::vec3(-1005.0, 0.0, 0.0);
				sphere.MaterialIndex = 1;
				m_scene.spheres.push_back(sphere);

				sphere.Position = glm::vec3(1005.0, 0.0, 0.0);
				sphere.MaterialIndex = 2;
				m_scene.spheres.push_back(sphere);

			}


			{
				Sphere& sphere = m_scene.spheres.emplace_back();
				sphere.Radius = 2.0f;
				sphere.MaterialIndex = 3;

				sphere.Position = glm::vec3(2.0, 2.0, -2.5);
			}		
		
			{
				Sphere& sphere = m_scene.spheres.emplace_back();
				sphere.Radius = 2.0f;
				sphere.MaterialIndex = 4;

				sphere.Position = glm::vec3(-2.0, 2.0, 0.0);
			}

			{
				Sphere& sphere = m_scene.spheres.emplace_back();
				sphere.Radius = 1.0f;
				sphere.MaterialIndex = 6;

				sphere.Position = glm::vec3(1.0, 0.5, 1.0);
			}
		}


		// Light
		{
			Sphere& sphere = m_scene.spheres.emplace_back();
			sphere.Radius = 20.0f;
			sphere.MaterialIndex = 5;

			sphere.Position = glm::vec3(0.0, 29.88, 0.0);
		}



		

		// Load bunny
		//if (m_objLoader.LoadFile("../Assets/cornell-box/CornellBox-Sphere.obj")) {

		tinyobj::ObjReader Reader;
		tinyobj::ObjReaderConfig config;
		config.triangulate = true;

		if (Reader.ParseFromFile("../Assets/cornell-box/CornellBox-Sphere.obj", config)) {
			auto& attrib = Reader.GetAttrib();
			auto& shapes = Reader.GetShapes();
			auto& materials = Reader.GetMaterials();

			// Loop over shapes
			for (size_t s = 0; s < shapes.size(); s++) {
				// Loop over faces(polygon)
				size_t index_offset = 0;
				for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
					size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
					Triangle tri;
					glm::vec3 avg_normal{ 0.0f };

					// Loop over vertices in the face.
					for (size_t v = 0; v < fv; v++) {
						// access to vertex
						tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
						tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
						tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
						tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

						float scale = 7.0f;
						tri.Vertices.push_back(glm::vec3(vx, vy, vz) * scale);

						// Check if `normal_index` is zero or positive. negative = no normal data
						if (idx.normal_index >= 0) {
							tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
							tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
							tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

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

					tri.Normal = glm::normalize(avg_normal);

					tri.MaterialIndex = 0;
					m_scene.triangles.push_back(tri);


					index_offset += fv;

					// per-face material
					//shapes[s].mesh.material_ids[f];
				}
			}
		}

		std::cout << "num tris: " << m_scene.triangles.size();
		
			//tinyobj::attrib_t attrib;
			//std::vector<tinyobj::shape_t> shapes;
			//std::vector<tinyobj::material_t> materials;

			//std::string warn;
			//std::string err;

			//if (tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "../Assets/cornell-box/CornellBox-Sphere.obj"))
			//{
			//	std::cout << "File loaded";
			//	//m_scene.meshes.push_back(m_objLoader.LoadedMeshes[0]);
			//}
		


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
		ImGui::Text("Last render: %.3fms", m_lastRenderTime);
		ImGui::Checkbox("Accumulate", &m_renderer.GetSettings().Accumulate);
		ImGui::DragInt("# Bounces", (int*) & m_renderer.GetSettings().Bounces,0.05f, 0);


		//ImGui::SliderFloat3("Light Position:", glm::value_ptr(m_scene.lightPosition), -10.0f, 10.0f, "%.2f");
		//ImGui::SliderFloat("Light Power:", &m_scene.lightPower, -1.0f, 2.0f, "%.2f");
		ImGui::End();

		// Materials
		ImGui::Begin("Materials");

		for (size_t i = 0; i < m_scene.materials.size(); i++)
		{
			ImGui::PushID((int)i);
			ImGui::Text("Material %i", i);
			ImGui::ColorEdit3("Albedo", glm::value_ptr(m_scene.materials[i].Albedo));
			ImGui::DragFloat("Emission", &m_scene.materials[i].Emission, 0.01f, 0.0f);
			ImGui::DragFloat("Roughness", &m_scene.materials[i].Roughness, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Metallic", &m_scene.materials[i].Metallic, 0.01f, 0.0f, 1.0f);
			ImGui::PopID();
			ImGui::Separator();
			ImGui::Separator();
		}

		ImGui::End();

		// Walls
		ImGui::Begin("Walls");
		for (size_t i = 0; i < 6; i++)
		{
			ImGui::PushID((int)i);
			ImGui::SliderInt("Material Index", (int*)&m_scene.spheres[i].MaterialIndex, 0, (int)m_scene.materials.size() - 1);
			ImGui::PopID();
			ImGui::Separator();
		}
		

		ImGui::End();

		// Spheres
		ImGui::Begin("Spheres");
		for (size_t i = 5; i < m_scene.spheres.size(); i++)
		{
			ImGui::PushID((int)i);
			ImGui::DragFloat3("Position", glm::value_ptr(m_scene.spheres[i].Position), 0.1f);
			ImGui::DragFloat("Radius", &m_scene.spheres[i].Radius, 0.01f, 0.1f, 5.0f);
			ImGui::SliderInt("Material Index", (int*) & m_scene.spheres[i].MaterialIndex, 0, (int)m_scene.materials.size() - 1);
			ImGui::PopID();
			ImGui::Separator();
			ImGui::Separator();
		}

		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");

		m_viewportWidth = (uint32_t) ImGui::GetContentRegionAvail().x;
		m_viewportHeight = (uint32_t) ImGui::GetContentRegionAvail().y;

		auto img = m_renderer.GetImage();
		if (img) {
			ImGui::Image(img->GetDescriptorSet(), { (float)img->GetWidth(), (float)img->GetHeight()}, ImVec2(0,1), ImVec2(1,0));
		}

		ImGui::End();
		ImGui::PopStyleVar();


		//ImGui::ShowDemoWindow();

		// Render every frame
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

	uint32_t m_viewportWidth, m_viewportHeight;

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
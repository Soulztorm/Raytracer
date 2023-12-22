#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Random.h"
#include "Walnut/Timer.h"

#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"

using namespace Walnut;

class ExampleLayer : public Walnut::Layer
{
public:
	ExampleLayer() : 
		m_camera(45.0f, 0.1f, 100.0f)
	{
		float maxSpread = 5.0f;
		float minSize = 1.5f;
		float maxSize = 10.0f;

		for (size_t i = 0; i < 20; i++)
		{
			Sphere sphere;
			sphere.Position = Random::Vec3(-maxSpread, maxSpread);
			sphere.Radius = std::min(Random::Float() * maxSize, minSize);
			sphere.Color = Random::Vec3();

			m_scene.spheres.push_back(sphere);
		}
	}

	virtual void OnUpdate(float ts) override
	{
		m_camera.OnUpdate(ts);
			//m_Renderer.ResetFrameIndex();
	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Settings");
		ImGui::Text("Last render: %.3fms", m_lastRenderTime);

		ImGui::SliderFloat3("Light Position:", &m_lightPos[0], -10.0f, 10.0f, "%.2f");
		m_renderer.SetLightPos(glm::vec3(m_lightPos[0], m_lightPos[1], m_lightPos[2]));

		ImGui::SliderFloat("Light Power:", &m_lightPower, -1.0f, 2.0f, "%.2f");
		m_renderer.SetLightPower(m_lightPower);

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
	float m_lightPos[3] = { -1.0f, 0.0f, 2.0f };
	float m_lightPower = 0.0f;
};










Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Walnut Example";
	spec.Width = 1280;
	spec.Height = 720;

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	app->SetMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	return app;
}
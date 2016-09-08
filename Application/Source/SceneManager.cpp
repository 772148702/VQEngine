//	DX11Renderer - VDemo | DirectX11 Renderer
//	Copyright(C) 2016  - Volkan Ilbeyli
//
//	This program is free software : you can redistribute it and / or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program.If not, see <http://www.gnu.org/licenses/>.
//
//	Contact: volkanilbeyli@gmail.com

#include "SceneManager.h"
#include "Engine.h"
#include "Input.h"
#include "Renderer.h"
#include "Camera.h"
#include "utils.h"

//#include <algorithm>
//#include <random>

#define MAX_LIGHTS 50
#define RAND_LIGHT_COUNT 30
#define DISCO_PERIOD 0.09

SceneManager::SceneManager()
{}

SceneManager::~SceneManager()
{}

void SceneManager::Initialize(Renderer * renderer, RenderData rData, Camera* cam)
{
	m_renderer = renderer;
	m_renderData = rData;
	m_camera = cam;
	InitializeBuilding();
	InitializeLights();
	
	m_centralObj.m_model.m_mesh = MESH_TYPE::GRID;
}

void SceneManager::InitializeBuilding()
{
	const float floorWidth = 19.0f;
	const float floorDepth = 30.0f;
	const float wallHieght = 15.0f;	// amount from middle to top and bottom: because gpu cube is 2 units in length

									// FLOOR
	{
		Transform& tf = m_floor.m_transform;
		tf.SetScale(floorWidth, 0.1f, floorDepth);
		tf.SetPosition(0, -wallHieght, 0);
		tf.SetRotationDeg(0.0f, 0.0f, 0.0f);
		m_floor.m_model.m_material.color = Color::green;
	}
	// CEILING
	{
		Transform& tf = m_ceiling.m_transform;
		tf.SetScale(floorWidth, 0.1f, floorDepth);
		tf.SetPosition(0, wallHieght, 0);
		tf.SetRotationDeg(0.0f, 0.0f, 0.0f);
		m_ceiling.m_model.m_material.color = Color::purple;
	}

	// RIGHT WALL
	{
		Transform& tf = m_wallR.m_transform;
		tf.SetScale(wallHieght, 0.1f, floorDepth);
		tf.SetPosition(floorWidth, 0, 0);
		tf.SetRotationDeg(0.0f, 0.0f, 90.0f);
		m_wallR.m_model.m_material.color = Color::gray;
	}

	// LEFT WALL
	{
		Transform& tf = m_wallL.m_transform;
		tf.SetScale(wallHieght, 0.1f, floorDepth);
		tf.SetPosition(-floorWidth, 0, 0);
		tf.SetRotationDeg(0.0f, 0.0f, -90.0f);
		m_wallL.m_model.m_material.color = Color::gray;
	}
	// WALL
	{
		Transform& tf = m_wallF.m_transform;
		tf.SetScale(floorWidth, 0.1f, wallHieght);
		tf.SetPosition(0, 0, floorDepth);
		tf.SetRotationDeg(90.0f, 0.0f, 0.0f);
		m_wallF.m_model.m_material.color = Color::white;
	}
}

void SceneManager::InitializeLights()
{
	{
		Light l;
		l.tf.SetPosition(-8.0f, 10.0f, 0);
		l.tf.SetScaleUniform(0.1f);
		l.model.m_material.color = Color::orange;
		l.color_ = Color::orange;
		l.SetLightRange(50);
		m_lights.push_back(l);
	}
	{
		Light l;
		l.tf.SetPosition(8.0f, -8.0f, 17.0f);
		l.tf.SetScaleUniform(0.1f);
		l.model.m_material.color = Color::white;
		l.color_ = Color::white;
		l.SetLightRange(30);
		m_lights.push_back(l);
	}

	for (size_t i = 0; i < RAND_LIGHT_COUNT; i++)
	{
		unsigned rndIndex = rand() % Color::Palette().size();
		Color rndColor = Color::Palette()[rndIndex];
		Light l;
		float x = RandF(-20.0f, 20.0f);
		float y = RandF(-20.0f, 20.0f);
		float z = RandF(-10.0f, 20.0f);
		l.tf.SetPosition(x, y, z);
		l.tf.SetScaleUniform(0.1f);
		l.model.m_material.color = l.color_ = rndColor;
		l.SetLightRange(5);
		m_lights.push_back(l);
	}
}

void SceneManager::Update(float dt)
{
	XMVECTOR rot = XMVectorZero();
	XMVECTOR tr = XMVectorZero();
	if (ENGINE->INP()->IsKeyDown('O')) rot += XMVectorSet(45.0f, 0.0f, 0.0f, 1.0f);
	if (ENGINE->INP()->IsKeyDown('P')) rot += XMVectorSet(0.0f, 45.0f, 0.0f, 1.0f);
	if (ENGINE->INP()->IsKeyDown('U')) rot += XMVectorSet(0.0f, 0.0f, 45.0f, 1.0f);

	if (ENGINE->INP()->IsKeyDown('L')) tr += XMVectorSet(45.0f, 0.0f, 0.0f, 1.0f);
	if (ENGINE->INP()->IsKeyDown('J')) tr += XMVectorSet(-45.0f, 0.0f, 0.0f, 1.0f);
	if (ENGINE->INP()->IsKeyDown('K')) tr += XMVectorSet(0.0f, 0.0f, -45.0f, 1.0f);
	if (ENGINE->INP()->IsKeyDown('I')) tr += XMVectorSet(0.0f, 0.0f, +45.0f, 1.0f);
	m_centralObj.m_transform.Rotate(rot * dt * 0.1f);
	m_lights[0].tf.Translate(tr * dt * 0.2f);

	static double accumulator = 0;
	accumulator += dt;
	if (accumulator > DISCO_PERIOD)
	{
		// shuffling won't rearrange data, just the means of indexing.
		//char info[256];
		//sprintf_s(info, "Shuffle(L1:(%f, %f, %f)\tL2:(%f, %f, %f)\n",
		//	m_lights[0].tf.GetPositionF3().x,
		//	m_lights[0].tf.GetPositionF3().y,
		//	m_lights[0].tf.GetPositionF3().z,
		//	m_lights[1].tf.GetPositionF3().x,
		//	m_lights[1].tf.GetPositionF3().y,
		//	m_lights[1].tf.GetPositionF3().z);
		//OutputDebugString(info);
		//static auto engine = std::default_random_engine{};
		//std::shuffle(std::begin(m_lights), std::end(m_lights), engine);

		// randomize all lights
		//for (Light& l : m_lights)
		//{
		//	size_t i = rand() % Color::Palette().size();
		//	Color c = Color::Color::Palette()[i];
		//	l.color_ = c;
		//	l.model.m_material.color = c;
		//}

		// randomize all lights except 1 and 2
		for (int j = 2; j<m_lights.size(); ++j)
		{
			Light& l = m_lights[j];
			size_t i = rand() % Color::Palette().size();
			Color c = Color::Color::Palette()[i];
			l.color_ = c;
			l.model.m_material.color = c;
		}

		accumulator = 0;
	}
}

void SceneManager::Render(const XMMATRIX& view, const XMMATRIX& proj) 
{
	RenderLights(view, proj);
	RenderBuilding(view, proj);

	// central obj
	m_renderer->SetBufferObj(MESH_TYPE::GRID);
	m_renderer->SetShader(m_renderData.texCoordShader);
	m_renderer->SetConstant4x4f("view", view);
	m_renderer->SetConstant4x4f("proj", proj);

	m_centralObj.m_transform.SetScale(XMFLOAT3(3 * 4, 5 * 4, 2 * 4));
	XMMATRIX world = m_centralObj.m_transform.WorldTransformationMatrix();
	m_renderer->SetConstant4x4f("world", world);
	m_renderer->Apply();
	m_renderer->DrawIndexed();


	m_renderer->SetBufferObj(MESH_TYPE::SPHERE);
	m_centralObj.m_transform.SetScale(XMFLOAT3(1, 1, 1));

	m_renderer->SetShader(m_renderData.normalShader);
	m_renderer->SetConstant4x4f("view", view);
	m_renderer->SetConstant4x4f("proj", proj);

	m_centralObj.m_transform.Translate(XMVectorSet(10.0f, 0.0f, 0.0f, 0.0f));
	world = m_centralObj.m_transform.WorldTransformationMatrix();
	m_renderer->SetConstant4x4f("world", world);
	m_renderer->Apply();
	m_renderer->DrawIndexed();

	m_renderer->SetShader(m_renderData.texCoordShader);
	m_renderer->SetConstant4x4f("view", view);
	m_renderer->SetConstant4x4f("proj", proj);

	m_centralObj.m_transform.Translate(XMVectorSet(-20.0f, 0.0f, 0.0f, 0.0f));
	world = m_centralObj.m_transform.WorldTransformationMatrix();
	m_renderer->SetConstant4x4f("world", world);
	m_renderer->Apply();
	m_renderer->DrawIndexed();
	m_centralObj.m_transform.Translate(XMVectorSet(10.0f, 0.0f, 0.0f, 0.0f));


}



void SceneManager::RenderBuilding(const XMMATRIX& view, const XMMATRIX& proj) const
{
	//XMFLOAT4 camPos  = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	//XMFLOAT4 camPos2 = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	//XMFLOAT4 camPos3 = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	//XMFLOAT4 camPos4 = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	//XMStoreFloat4(&camPos, -view.r[0]);
	//XMStoreFloat4(&camPos2, -view.r[1]);
	//XMStoreFloat4(&camPos3, -view.r[2]);
	//XMStoreFloat4(&camPos4, -view.r[3]);

	//char info[128];
	//sprintf_s(info, "camPos: (%f, %f, %f, %f)\n", camPos.x, camPos.y, camPos.z, camPos.w);
	//OutputDebugString(info);
	//sprintf_s(info, "camPos2: (%f, %f, %f, %f)\n", camPos2.x, camPos2.y, camPos2.z, camPos2.w);
	//OutputDebugString(info);
	//sprintf_s(info, "camPos3: (%f, %f, %f, %f)\n", camPos3.x, camPos3.y, camPos3.z, camPos3.w);
	//OutputDebugString(info);
	//sprintf_s(info, "camPos4: (%f, %f, %f, %f)\n", camPos4.x, camPos4.y, camPos4.z, camPos4.w);
	//OutputDebugString(info);
	

	XMFLOAT3 camPos_real = m_camera->GetPositionF();
	//sprintf_s(info, "camPos_real: (%f, %f, %f)\n\n", camPos_real.x, camPos_real.y, camPos_real.z);
	//OutputDebugString(info);

	m_renderer->SetShader(m_renderData.phongShader); 
	//m_renderer->SetShader(m_renderData.normalShader); 
	m_renderer->SetConstant4x4f("view", view);
	m_renderer->SetConstant4x4f("proj", proj);
	m_renderer->SetBufferObj(MESH_TYPE::CUBE);

	m_renderer->SetConstant3f("cameraPos", camPos_real);

	// send light information
	std::vector<ShaderLight> lights;
	const ShaderLight defaultLight = ShaderLight();
	for (const Light& l : m_lights)
	{
		lights.push_back(l.ShaderLightStruct());
	}
	while (lights.size() < MAX_LIGHTS) lights.push_back(defaultLight);
	m_renderer->SetConstant1f("lightCount", static_cast<float>(m_lights.size()));
	m_renderer->SetConstantStruct("lights", static_cast<void*>(lights.data()));

#ifdef _DEBUG
	if (lights.size() > MAX_LIGHTS)
		OutputDebugString("Warning: light count larger than MAX_LIGHTS\n");
#endif

	{
		XMMATRIX world = m_floor.m_transform.WorldTransformationMatrix();
		XMFLOAT3 color = m_floor.m_model.m_material.color.Value();
		m_renderer->SetConstant4x4f("world", world);
		m_renderer->SetConstant3f("color", color);
		m_renderer->Apply();
		m_renderer->DrawIndexed();
	}
	{
		XMMATRIX world = m_ceiling.m_transform.WorldTransformationMatrix();
		XMFLOAT3 color = m_ceiling.m_model.m_material.color.Value();
		m_renderer->SetConstant4x4f("world", world);
		m_renderer->SetConstant3f("color", color);
		m_renderer->Apply();
		m_renderer->DrawIndexed();
	}
	{
		XMMATRIX world = m_wallR.m_transform.WorldTransformationMatrix();
		XMFLOAT3 color = m_wallR.m_model.m_material.color.Value();
		m_renderer->SetConstant4x4f("world", world);
		m_renderer->SetConstant3f("color", color);
		m_renderer->Apply();
		m_renderer->DrawIndexed();
	}
	{
		XMMATRIX world = m_wallL.m_transform.WorldTransformationMatrix();
		XMFLOAT3 color = m_wallL.m_model.m_material.color.Value();
		m_renderer->SetConstant4x4f("world", world);
		m_renderer->SetConstant3f("color", color);
		m_renderer->Apply();
		m_renderer->DrawIndexed();
	}
	{
		XMMATRIX world = m_wallF.m_transform.WorldTransformationMatrix();
		XMFLOAT3 color = m_wallF.m_model.m_material.color.Value();
		m_renderer->SetConstant4x4f("world", world);
		m_renderer->SetConstant3f("color", color);
		m_renderer->Apply();
		m_renderer->DrawIndexed();
	}
}

void SceneManager::RenderLights(const XMMATRIX& view, const XMMATRIX& proj) const
{
	m_renderer->Reset();
	m_renderer->SetShader(m_renderData.unlitShader);
	m_renderer->SetConstant4x4f("view", view);
	m_renderer->SetConstant4x4f("proj", proj);
	m_renderer->SetBufferObj(MESH_TYPE::SPHERE);
	for (const Light& light : m_lights)
	{
		XMMATRIX world = light.tf.WorldTransformationMatrix();
		XMFLOAT3 color = light.model.m_material.color.Value();
		m_renderer->SetConstant4x4f("world", world);
		m_renderer->SetConstant3f("color", color);
		m_renderer->Apply();
		m_renderer->DrawIndexed();
	}
}
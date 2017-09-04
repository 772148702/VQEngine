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

#pragma once

#include "RenderingEnums.h"
#include "Settings.h"

#include <array>
#include <vector>
#include <memory>
using std::shared_ptr;

#include <DirectXMath.h> // todo wrap utils/math
using namespace DirectX;

class Camera;
class Renderer;
class GameObject;
struct Light;
struct ID3D11Device;

struct SceneLightData;
struct SceneView
{
	XMMATRIX			viewProj;
	shared_ptr<Camera>  pCamera;
};

struct ShadowMapPass
{
	void Initialize(
		Renderer*					pRenderer, 
		ID3D11Device*				device, 
		const Settings::ShadowMap&	shadowMapSettings
	);
	
	void RenderShadowMaps(
		Renderer*						pRenderer, 
		const std::vector<const Light*> shadowLights, 
		const std::vector<GameObject*> ZPassObjects
	) const;
	
	unsigned				_shadowMapDimension;
	TextureID				_shadowMap;
	SamplerID				_shadowSampler;
	ShaderID				_shadowShader;
	RasterizerStateID		_drawRenderState;
	RasterizerStateID		_shadowRenderState;
	D3D11_VIEWPORT			_shadowViewport;
	DepthTargetID			_shadowDepthTarget;
};

struct BloomPass
{
	inline void ToggleBloomPass() { _isEnabled = !_isEnabled; }
	BloomPass() : 
		_blurSampler(-1), 
		_colorRT(-1), 
		_brightRT(-1), 
		_blurPingPong({ -1, -1 }), 
		_isEnabled(true) 
	{}

	SamplerID						_blurSampler;
	RenderTargetID					_colorRT;
	RenderTargetID					_brightRT;
	RenderTargetID					_finalRT;
	std::array<RenderTargetID, 2>	_blurPingPong;
	bool							_isEnabled;
};

struct TonemappingCombinePass
{
	RenderTargetID						_finalRenderTarget;
};

struct PostProcessPass
{
	void Initialize(
		Renderer*						pRenderer, 
		const Settings::PostProcess&	postProcessSettings
	);
	void Render(Renderer* pRenderer) const;

	RenderTargetID				_worldRenderTarget;
	BloomPass					_bloomPass;
	TonemappingCombinePass		_tonemappingPass;
	Settings::PostProcess		_settings;
};

struct GBuffer
{
	RenderTargetID	_diffuseRoughnessRT;
	RenderTargetID	_specularMetallicRT;
	RenderTargetID	_normalRT;
	RenderTargetID	_positionRT;	// todo: construct position from viewProj^-1 
};

struct DeferredRenderingPasses
{
	void InitializeGBuffer(Renderer* pRenderer);
	void SetGeometryRenderingStates(Renderer* pRenderer) const;
	void RenderLightingPass(
		Renderer*				pRenderer, 
		const RenderTargetID	target, 
		const SceneView&		sceneView, 
		const SceneLightData&	lights
	) const;

	GBuffer _GBuffer;
};
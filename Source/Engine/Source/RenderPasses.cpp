//	VQEngine | DirectX11 Renderer
//	Copyright(C) 2018  - Volkan Ilbeyli
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


#include "RenderPasses.h"
#include "Engine.h"
#include "GameObject.h"
#include "Light.h"

#include "Renderer/Renderer.h"
#include "Renderer/D3DManager.h"

#include "Utilities/Camera.h"
#include "Utilities/Log.h"


#include <array>

void ShadowMapPass::InitializeSpotLightShadowMaps(Renderer* pRenderer, const Settings::ShadowMap& shadowMapSettings)
{
	this->mShadowMapDimension_Spot = static_cast<int>(shadowMapSettings.dimension);

#if _DEBUG
	pRenderer->m_Direct3D->ReportLiveObjects("--------SHADOW_PASS_INIT");
#endif

	// check feature support & error handle:
	// https://msdn.microsoft.com/en-us/library/windows/apps/dn263150
	const bool bDepthOnly = true;
	const EImageFormat format = bDepthOnly ? R32 : R24G8;

	DepthTargetDesc depthDesc;
	depthDesc.format = bDepthOnly ? D32F : D24UNORM_S8U;
	TextureDesc& texDesc = depthDesc.textureDesc;
	texDesc.format = format;
	texDesc.usage = static_cast<ETextureUsage>(DEPTH_TARGET | RESOURCE);

	// CREATE DEPTH TARGETS: SPOT LIGHTS
	//
	texDesc.height = texDesc.width = mShadowMapDimension_Spot;
	texDesc.arraySize = NUM_SPOT_LIGHT_SHADOW;
	auto DepthTargetIDs = pRenderer->AddDepthTarget(depthDesc);
	this->mDepthTargets_Spot.resize(NUM_SPOT_LIGHT_SHADOW);
	std::copy(RANGE(DepthTargetIDs), this->mDepthTargets_Spot.begin());

	this->mShadowMapTextures_Spot = this->mDepthTargets_Spot.size() > 0
		? pRenderer->GetDepthTargetTexture(this->mDepthTargets_Spot[0])
		: -1;

	this->mShadowMapShader = EShaders::SHADOWMAP_DEPTH;

	ZeroMemory(&mShadowViewPort_Spot, sizeof(D3D11_VIEWPORT));
	this->mShadowViewPort_Spot.Height = static_cast<float>(mShadowMapDimension_Spot);
	this->mShadowViewPort_Spot.Width = static_cast<float>(mShadowMapDimension_Spot);
	this->mShadowViewPort_Spot.MinDepth = 0.f;
	this->mShadowViewPort_Spot.MaxDepth = 1.f;

}

void ShadowMapPass::InitializeDirectionalLightShadowMap(Renderer * pRenderer, const Settings::ShadowMap & shadowMapSettings)
{
	const int textureDimension = static_cast<int>(shadowMapSettings.dimension);
	const bool bDepthOnly = true;
	const EImageFormat format = bDepthOnly ? R32 : R24G8;

	DepthTargetDesc depthDesc;
	depthDesc.format = bDepthOnly ? D32F : D24UNORM_S8U;
	TextureDesc& texDesc = depthDesc.textureDesc;
	texDesc.format = format;
	texDesc.usage = static_cast<ETextureUsage>(DEPTH_TARGET | RESOURCE);

	texDesc.height = texDesc.width = textureDimension;
	texDesc.arraySize = 1;
	
#if 0
	// first time - add target
	if (this->mDepthTarget_Directional == -1)
	{
		this->mDepthTarget_Directional = pRenderer->AddDepthTarget(depthDesc)[0];
		this->mShadowMapTexture_Directional = pRenderer->GetDepthTargetTexture(this->mDepthTarget_Directional);
	}
	else // other times - resize target
	{
		pRenderer->ResizeDepthTarget(this->mDepthTarget_Directional, depthDesc);
	}
#else
	// always add depth target when loading a new scene
	//
	this->mDepthTarget_Directional = pRenderer->AddDepthTarget(depthDesc)[0];
	this->mShadowMapTexture_Directional = pRenderer->GetDepthTargetTexture(this->mDepthTarget_Directional);
#endif

	mShadowViewPort_Directional.Width  = static_cast<FLOAT>(textureDimension);
	mShadowViewPort_Directional.Height = static_cast<FLOAT>(textureDimension);
	mShadowViewPort_Directional.MinDepth = 0.f;
	mShadowViewPort_Directional.MaxDepth = 1.f;
}

void ShadowMapPass::RenderShadowMaps(Renderer* pRenderer, const ShadowView& shadowView, GPUProfiler* pGPUProfiler) const
{
	const bool bNoShadowingLights = shadowView.spots.empty() && shadowView.points.empty() && shadowView.pDirectional == nullptr;
	if (bNoShadowingLights) return;

	//pRenderer->BindRenderTarget(0);
	pRenderer->SetDepthStencilState(EDefaultDepthStencilState::DEPTH_WRITE);
	pRenderer->SetShader(mShadowMapShader);					// shader for rendering z buffer
	pRenderer->SetViewport(mShadowViewPort_Spot);

	// SPOT LIGHT SHADOW MAPS
	//
	pGPUProfiler->BeginEntry("Spots");
	for (size_t i = 0; i < shadowView.spots.size(); i++)
	{
#if _DEBUG
		if (shadowView.shadowMapRenderListLookUp.find(shadowView.spots[i]) == shadowView.shadowMapRenderListLookUp.end())
		{
			Log::Error("Spot light not found in shadowmap render list lookup");
			continue;;
		}
#endif

		pRenderer->BindDepthTarget(mDepthTargets_Spot[i]);	// only depth stencil buffer
		pRenderer->BeginRender(ClearCommand::Depth(1.0f));
		pRenderer->SetConstant4x4f("viewProj", shadowView.spots[i]->GetLightSpaceMatrix());
		pRenderer->SetConstant4x4f("view"    , shadowView.spots[i]->GetViewMatrix());
		pRenderer->SetConstant4x4f("proj"    , shadowView.spots[i]->GetProjectionMatrix());
		pRenderer->Apply();

		pRenderer->BeginEvent("Spot[" + std::to_string(i)  + "]: DrawSceneZ()");
		for (const GameObject* obj : shadowView.shadowMapRenderListLookUp.at(shadowView.spots[i]))
			obj->RenderZ(pRenderer);
		pRenderer->EndEvent();
	}
	pGPUProfiler->EndEntry();

	// DIRECTIONAL SHADOW MAP
	//
	if (shadowView.pDirectional != nullptr)
	{
		pGPUProfiler->BeginEntry("Directional");
		pRenderer->SetViewport(mShadowViewPort_Directional);
		pRenderer->BindDepthTarget(mDepthTarget_Directional);	// only depth stencil buffer
		pRenderer->Apply();
		pRenderer->BeginRender(ClearCommand::Depth(1.0f));
		pRenderer->SetConstant4x4f("viewProj", shadowView.pDirectional->GetLightSpaceMatrix());
		pRenderer->SetConstant4x4f("view", shadowView.pDirectional->GetViewMatrix());
		pRenderer->SetConstant4x4f("proj", shadowView.pDirectional->GetProjectionMatrix());
		pRenderer->Apply();

		pRenderer->BeginEvent("Directional: DrawSceneZ()");
		for (const GameObject* obj : shadowView.casters)
			obj->RenderZ(pRenderer);
		pRenderer->EndEvent();
		pGPUProfiler->EndEntry();
	}
}



void PostProcessPass::Initialize(Renderer* pRenderer, const Settings::PostProcess& postProcessSettings)
{
	_settings = postProcessSettings;
	DXGI_SAMPLE_DESC smpDesc;
	smpDesc.Count = 1;
	smpDesc.Quality = 0;

	constexpr const EImageFormat HDR_Format = RGBA16F;
	constexpr const EImageFormat LDR_Format = RGBA8UN;

	const EImageFormat imageFormat = _settings.HDREnabled ? HDR_Format : LDR_Format;

	RenderTargetDesc rtDesc = {};
	rtDesc.textureDesc.width = pRenderer->WindowWidth();
	rtDesc.textureDesc.height = pRenderer->WindowHeight();
	rtDesc.textureDesc.mipCount = 1;
	rtDesc.textureDesc.arraySize = 1;
	rtDesc.textureDesc.format = imageFormat;
	rtDesc.textureDesc.usage = ETextureUsage::RENDER_TARGET_RW;
	rtDesc.format = imageFormat;

	// Bloom effect
	this->_bloomPass._colorRT = pRenderer->AddRenderTarget(rtDesc);
	this->_bloomPass._brightRT = pRenderer->AddRenderTarget(rtDesc);
	this->_bloomPass._finalRT = pRenderer->AddRenderTarget(rtDesc);
	this->_bloomPass._blurPingPong[0] = pRenderer->AddRenderTarget(rtDesc);
	this->_bloomPass._blurPingPong[1] = pRenderer->AddRenderTarget(rtDesc);

	const char* pFSQ_VS = "FullScreenquad_vs.hlsl";
	const ShaderDesc bloomPipelineShaders[] = 
	{
		ShaderDesc{"Bloom", 
			ShaderStageDesc{pFSQ_VS   , {} },
			ShaderStageDesc{"Bloom_ps.hlsl", {} },
		},
		ShaderDesc{"Blur",
			ShaderStageDesc{pFSQ_VS  , {} },
			ShaderStageDesc{"Blur_ps.hlsl", {} },
		},
		ShaderDesc{"BloomCombine",
			ShaderStageDesc{pFSQ_VS          , {} },
			ShaderStageDesc{"BloomCombine_ps.hlsl", {} },
		},
	};
	this->_bloomPass._bloomFilterShader  = pRenderer->CreateShader(bloomPipelineShaders[0]);
	this->_bloomPass._blurShader		 = pRenderer->CreateShader(bloomPipelineShaders[1]);
	this->_bloomPass._bloomCombineShader = pRenderer->CreateShader(bloomPipelineShaders[2]);

	D3D11_SAMPLER_DESC blurSamplerDesc = {};
	blurSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	blurSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	blurSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	blurSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	this->_bloomPass._blurSampler = pRenderer->CreateSamplerState(blurSamplerDesc);
	
	// Tonemapping
	this->_tonemappingPass._finalRenderTarget = pRenderer->GetDefaultRenderTarget();

	const ShaderDesc tonemappingShaderDesc = ShaderDesc{"Tonemapping",
		ShaderStageDesc{ pFSQ_VS         , {} },
		ShaderStageDesc{ "Tonemapping_ps.hlsl", {} }
	};
	this->_tonemappingPass._toneMappingShader = pRenderer->CreateShader(tonemappingShaderDesc);

	// World Render Target
	this->_worldRenderTarget = pRenderer->AddRenderTarget(rtDesc);
}

void PostProcessPass::Render(Renderer * pRenderer, bool bBloomOn) const
{
	pRenderer->BeginEvent("Post Processing");

	const TextureID worldTexture = pRenderer->GetRenderTargetTexture(_worldRenderTarget);
	const auto IABuffersQuad = ENGINE->GetGeometryVertexAndIndexBuffers(EGeometry::FULLSCREENQUAD);

	// ======================================================================================
	// BLOOM  PASS
	// ======================================================================================
	const Settings::Bloom& sBloom = _settings.bloom;
	const bool bBloom = bBloomOn && _settings.bloom.bEnabled;
	if (bBloom)
	{
		const ShaderID currentShader = pRenderer->GetActiveShader();

		// bright filter
		pRenderer->BeginEvent("Bloom Bright Filter");
		pRenderer->SetShader(_bloomPass._bloomFilterShader);
		pRenderer->BindRenderTargets(_bloomPass._colorRT, _bloomPass._brightRT);
		pRenderer->UnbindDepthTarget();
		pRenderer->SetVertexBuffer(IABuffersQuad.first);
		pRenderer->SetIndexBuffer(IABuffersQuad.second);
		pRenderer->Apply();
		pRenderer->SetTexture("worldRenderTarget", worldTexture);
		pRenderer->SetConstant1f("BrightnessThreshold", sBloom.brightnessThreshold);
		pRenderer->Apply();
		pRenderer->DrawIndexed();
		pRenderer->EndEvent();

		// blur
		const TextureID brightTexture = pRenderer->GetRenderTargetTexture(_bloomPass._brightRT);
		pRenderer->BeginEvent("Bloom Blur Pass");
		pRenderer->SetShader(_bloomPass._blurShader);
		for (int i = 0; i < 5; ++i)
		{
			const int isHorizontal = i % 2;
			const TextureID pingPong = pRenderer->GetRenderTargetTexture(_bloomPass._blurPingPong[1 - isHorizontal]);
			const int texWidth = pRenderer->GetTextureObject(pingPong)._width;
			const int texHeight = pRenderer->GetTextureObject(pingPong)._height;

			pRenderer->UnbindRenderTargets();
			pRenderer->Apply();
			pRenderer->BindRenderTarget(_bloomPass._blurPingPong[isHorizontal]);
			pRenderer->SetConstant1i("isHorizontal", 1 - isHorizontal);
			pRenderer->SetConstant1i("textureWidth", texWidth);
			pRenderer->SetConstant1i("textureHeight", texHeight);
			pRenderer->SetTexture("InputTexture", i == 0 ? brightTexture : pingPong);
			pRenderer->SetSamplerState("BlurSampler", _bloomPass._blurSampler);
			pRenderer->Apply();
			pRenderer->DrawIndexed();
		}
		pRenderer->EndEvent();

		// additive blend combine
		const TextureID colorTex = pRenderer->GetRenderTargetTexture(_bloomPass._colorRT);
		const TextureID bloomTex = pRenderer->GetRenderTargetTexture(_bloomPass._blurPingPong[0]);
		pRenderer->BeginEvent("Bloom Combine");
		pRenderer->SetShader(_bloomPass._bloomCombineShader);
		pRenderer->BindRenderTarget(_bloomPass._finalRT);
		pRenderer->Apply();
		pRenderer->SetTexture("ColorTexture", colorTex);
		pRenderer->SetTexture("BloomTexture", bloomTex);
		pRenderer->SetSamplerState("BlurSampler", _bloomPass._blurSampler);
		pRenderer->Apply();
		pRenderer->DrawIndexed();
		pRenderer->EndEvent();
	}



	// ======================================================================================
	// TONEMAPPING PASS
	// ======================================================================================
	const TextureID colorTex = pRenderer->GetRenderTargetTexture(bBloom ? _bloomPass._finalRT : _worldRenderTarget);
	const float isHDR = _settings.HDREnabled ? 1.0f : 0.0f;
	pRenderer->BeginEvent("Tonemapping");
	pRenderer->UnbindDepthTarget();
	pRenderer->SetShader(_tonemappingPass._toneMappingShader);
	pRenderer->SetVertexBuffer(IABuffersQuad.first);
	pRenderer->SetIndexBuffer(IABuffersQuad.second);
	pRenderer->SetSamplerState("Sampler", _bloomPass._blurSampler);
	pRenderer->SetConstant1f("exposure", _settings.toneMapping.exposure);
	pRenderer->SetConstant1f("isHDR", isHDR);
	pRenderer->BindRenderTarget(_tonemappingPass._finalRenderTarget);
	pRenderer->SetTexture("ColorTexture", colorTex);
	pRenderer->Apply();
	pRenderer->DrawIndexed();
	pRenderer->EndEvent();

	pRenderer->EndEvent();	// Post Process
}



void DeferredRenderingPasses::Initialize(Renderer * pRenderer)
{
	//const ShaderMacro testMacro = { "TEST_DEFINE", "1" };

	// TODO: reduce the code redundancy
	const char* pFSQ_VS = "FullScreenQuad_vs.hlsl";
	const char* pLight_VS = "MVPTransformationWithUVs_vs.hlsl";
	const ShaderDesc geomShaderDesc = { "GBufferPass", 
	{
		ShaderStageDesc{ "Deferred_Geometry_vs.hlsl", {} },
		ShaderStageDesc{ "Deferred_Geometry_ps.hlsl", {} }
	}};
	const ShaderDesc ambientShaderDesc = { "Deferred_Ambient",
	{
		ShaderStageDesc{ pFSQ_VS, {} },
		ShaderStageDesc{ "deferred_brdf_ambient_ps.hlsl", {} }
	}};
	const ShaderDesc ambientIBLShaderDesc = { "Deferred_AmbientIBL",
	{
		ShaderStageDesc{ pFSQ_VS, {} },
		ShaderStageDesc{ "deferred_brdf_ambientIBL_ps.hlsl", {} }
	}};
	const ShaderDesc BRDFLightingShaderDesc = { "Deferred_BRDF_Lighting",
	{
		ShaderStageDesc{ pFSQ_VS, {} },
		ShaderStageDesc{ "deferred_brdf_lighting_ps.hlsl", {} }
	}};
	const ShaderDesc phongLighintShaderDesc = { "Deferred_Phong_Lighting",
	{
		ShaderStageDesc{ pFSQ_VS, {} },
		ShaderStageDesc{ "deferred_phong_lighting_ps.hlsl", {} }
	}};
	const ShaderDesc BRDF_PointLightShaderDesc = { "Deferred_BRDF_Point",
	{
		ShaderStageDesc{ pLight_VS, {} },
		ShaderStageDesc{ "deferred_brdf_pointLight_ps.hlsl", {} }
	}};
	const ShaderDesc BRDF_SpotLightShaderDesc = { "Deferred_BRDF_Spot",
	{
		ShaderStageDesc{ pLight_VS, {} },
		ShaderStageDesc{ "deferred_brdf_spotLight_ps.hlsl", {} }
	}};

	InitializeGBuffer(pRenderer);

	_geometryShader		 = pRenderer->CreateShader(geomShaderDesc);
	_ambientShader		 = pRenderer->CreateShader(ambientShaderDesc);
	_ambientIBLShader	 = pRenderer->CreateShader(ambientIBLShaderDesc);
	_BRDFLightingShader  = pRenderer->CreateShader(BRDFLightingShaderDesc);
	_phongLightingShader = pRenderer->CreateShader(phongLighintShaderDesc);
	_spotLightShader	 = pRenderer->CreateShader(BRDF_PointLightShaderDesc);
	_pointLightShader	 = pRenderer->CreateShader(BRDF_SpotLightShaderDesc);

	// deferred geometry is accessed from elsewhere, needs to be globally defined
	assert(EShaders::DEFERRED_GEOMETRY == _geometryShader);	// this assumption may break, make sure it doesn't...
}

void DeferredRenderingPasses::InitializeGBuffer(Renderer* pRenderer)
{	
	EImageFormat imageFormats[2] = { RGBA32F, RGBA16F };
	RenderTargetDesc rtDesc[2] = { {}, {} };
	for (int i = 0; i < 2; ++i)
	{
		rtDesc[i].textureDesc.width = pRenderer->WindowWidth();
		rtDesc[i].textureDesc.height = pRenderer->WindowHeight();
		rtDesc[i].textureDesc.mipCount = 1;
		rtDesc[i].textureDesc.arraySize = 1;
		rtDesc[i].textureDesc.usage = ETextureUsage::RENDER_TARGET_RW;
		rtDesc[i].textureDesc.format = imageFormats[i];
		rtDesc[i].format = imageFormats[i];
	}

	constexpr size_t Float3TypeIndex = 0;		constexpr size_t Float4TypeIndex = 1;
	this->_GBuffer._normalRT		   = pRenderer->AddRenderTarget(rtDesc[Float3TypeIndex]);
	this->_GBuffer._diffuseRoughnessRT = pRenderer->AddRenderTarget(rtDesc[Float4TypeIndex]);
	this->_GBuffer._specularMetallicRT = pRenderer->AddRenderTarget(rtDesc[Float4TypeIndex]);
	this->_GBuffer.bInitialized = true;

	// http://download.nvidia.com/developer/presentations/2004/6800_Leagues/6800_Leagues_Deferred_Shading.pdf
	// Option: trade storage for computation
	//  - Store pos.z     and compute xy from z + window.xy		(implemented)
	//	- Store normal.xy and compute z = sqrt(1 - x^2 - y^2)
	//
	{	// Geometry depth stencil state descriptor
		D3D11_DEPTH_STENCILOP_DESC dsOpDesc = {};
		dsOpDesc.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsOpDesc.StencilDepthFailOp = D3D11_STENCIL_OP_INCR_SAT;
		dsOpDesc.StencilPassOp = D3D11_STENCIL_OP_INCR_SAT;
		dsOpDesc.StencilFunc = D3D11_COMPARISON_ALWAYS;

		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.FrontFace = dsOpDesc;
		
		//dsOpDesc.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		desc.BackFace = dsOpDesc;

		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

		desc.StencilEnable = true;
		desc.StencilReadMask  = 0xFF;
		desc.StencilWriteMask = 0xFF;

		_geometryStencilState = pRenderer->AddDepthStencilState(desc);
	}

}

void DeferredRenderingPasses::ClearGBuffer(Renderer* pRenderer)
{
	const bool bDoClearColor = true;
	const bool bDoClearDepth = false;
	const bool bDoClearStencil = false;
	ClearCommand clearCmd(
		bDoClearColor, bDoClearDepth, bDoClearStencil,
		{ 0, 0, 0, 0 }, 0, 0
	);
	pRenderer->BindRenderTargets(_GBuffer._diffuseRoughnessRT, _GBuffer._specularMetallicRT, _GBuffer._normalRT);
	pRenderer->BeginRender(clearCmd);
}

void DeferredRenderingPasses::SetGeometryRenderingStates(Renderer* pRenderer) const
{
	const bool bDoClearColor = true;
	const bool bDoClearDepth = true;
	const bool bDoClearStencil = true;
	ClearCommand clearCmd(
		bDoClearColor	, bDoClearDepth	, bDoClearStencil,
		{ 0, 0, 0, 0 }	, 1				, 0
	);

	pRenderer->SetShader(_geometryShader);
	pRenderer->BindRenderTargets(_GBuffer._diffuseRoughnessRT, _GBuffer._specularMetallicRT, _GBuffer._normalRT);
	pRenderer->BindDepthTarget(ENGINE->GetWorldDepthTarget());
	pRenderer->SetDepthStencilState(_geometryStencilState); 
	pRenderer->SetSamplerState("sNormalSampler", EDefaultSamplerState::LINEAR_FILTER_SAMPLER_WRAP_UVW);
	pRenderer->BeginRender(clearCmd);
	pRenderer->Apply();
}

void DeferredRenderingPasses::RenderLightingPass(
	Renderer* pRenderer, 
	const RenderTargetID target, 
	const SceneView& sceneView, 
	const SceneLightingData& lights, 
	const TextureID tSSAO, 
	bool bUseBRDFLighting) const
{
	ClearCommand cmd = ClearCommand::Color({ 0, 0, 0, 0 });

	const bool bAmbientOcclusionOn = tSSAO == -1;

	const vec2 screenSize = pRenderer->GetWindowDimensionsAsFloat2();
	const TextureID texNormal = pRenderer->GetRenderTargetTexture(_GBuffer._normalRT);
	const TextureID texDiffuseRoughness = pRenderer->GetRenderTargetTexture(_GBuffer._diffuseRoughnessRT);
	const TextureID texSpecularMetallic = pRenderer->GetRenderTargetTexture(_GBuffer._specularMetallicRT);
	const ShaderID lightingShader = bUseBRDFLighting ? _BRDFLightingShader : _phongLightingShader;
	const TextureID texIrradianceMap = sceneView.environmentMap.irradianceMap;
	const SamplerID smpEnvMap = sceneView.environmentMap.envMapSampler;
	const TextureID texSpecularMap = sceneView.environmentMap.prefilteredEnvironmentMap;
	const TextureID tBRDFLUT = EnvironmentMap::sBRDFIntegrationLUTTexture;
	const TextureID depthTexture = pRenderer->GetDepthTargetTexture(ENGINE->GetWorldDepthTarget());

	const auto IABuffersQuad = ENGINE->GetGeometryVertexAndIndexBuffers(EGeometry::FULLSCREENQUAD);

	// pRenderer->UnbindRendertargets();	// ignore this for now
	pRenderer->UnbindDepthTarget();
	pRenderer->BindRenderTarget(target);
	pRenderer->BeginRender(cmd);
	pRenderer->Apply();

	// AMBIENT LIGHTING
	//-----------------------------------------------------------------------------------------
	const bool bSkylight = sceneView.bIsIBLEnabled && texIrradianceMap != -1;
	if(bSkylight)
	{
		pRenderer->BeginEvent("Environment Map Lighting Pass");
		pRenderer->SetShader(_ambientIBLShader);
		pRenderer->SetTexture("tDiffuseRoughnessMap", texDiffuseRoughness);
		pRenderer->SetTexture("tSpecularMetalnessMap", texSpecularMetallic);
		pRenderer->SetTexture("tNormalMap", texNormal);
		pRenderer->SetTexture("tDepthMap", depthTexture);
		pRenderer->SetTexture("tAmbientOcclusion", tSSAO);
		pRenderer->SetTexture("tIrradianceMap", texIrradianceMap);
		pRenderer->SetTexture("tPreFilteredEnvironmentMap", texSpecularMap);
		pRenderer->SetTexture("tBRDFIntegrationLUT", tBRDFLUT);
		pRenderer->SetSamplerState("sEnvMapSampler", smpEnvMap);
		pRenderer->SetSamplerState("sWrapSampler", EDefaultSamplerState::WRAP_SAMPLER);
		pRenderer->SetConstant4x4f("matViewInverse", sceneView.viewInverse);
		pRenderer->SetConstant4x4f("matProjInverse", sceneView.projectionInverse);
	}
	else
	{
		pRenderer->BeginEvent("Ambient Pass");
		pRenderer->SetShader(_ambientShader);
		pRenderer->SetTexture("tDiffuseRoughnessMap", texDiffuseRoughness);
		pRenderer->SetTexture("tAmbientOcclusion", tSSAO);
	}
	pRenderer->SetSamplerState("sNearestSampler", EDefaultSamplerState::POINT_SAMPLER);
	pRenderer->SetConstant1f("ambientFactor", sceneView.sceneRenderSettings.ssao.ambientFactor);
	pRenderer->SetVertexBuffer(IABuffersQuad.first);
	pRenderer->SetIndexBuffer(IABuffersQuad.second);
	pRenderer->Apply();
	pRenderer->DrawIndexed();
	pRenderer->EndEvent();


	// DIFFUSE & SPECULAR LIGHTING
	//-----------------------------------------------------------------------------------------
	pRenderer->SetBlendState(EDefaultBlendState::ADDITIVE_COLOR);

	// draw fullscreen quad for lighting for now. Will add light volumes
	// as the scene gets more complex or depending on performance needs.
#ifdef USE_LIGHT_VOLUMES
#if 0
	const auto IABuffersSphere = ENGINE->GetGeometryVertexAndIndexBuffers(EGeometry::SPHERE);

	pRenderer->SetConstant3f("CameraWorldPosition", sceneView.pCamera->GetPositionF());
	pRenderer->SetConstant2f("ScreenSize", screenSize);
	pRenderer->SetTexture("texDiffuseRoughnessMap", texDiffuseRoughness);
	pRenderer->SetTexture("texSpecularMetalnessMap", texSpecularMetallic);
	pRenderer->SetTexture("texNormals", texNormal);

	// POINT LIGHTS
	pRenderer->SetShader(SHADERS::DEFERRED_BRDF_POINT);
	pRenderer->SetVertexBuffer(IABuffersSphere.first);
	pRenderer->SetIndexBuffer(IABuffersSphere.second);
	//pRenderer->SetRasterizerState(ERasterizerCullMode::)
	for(const Light& light : lights)
	{
		if (light._type == Light::ELightType::POINT)
		{
			const float& r   = light._range;	//bounding sphere radius
			const vec3&  pos = light._transform._position;
			const XMMATRIX world = {
				r, 0, 0, 0,
				0, r, 0, 0,
				0, 0, r, 0,
				pos.x(), pos.y(), pos.z(), 1,
			};

			const XMMATRIX wvp = world * sceneView.viewProj;
			const LightShaderSignature lightData = light.ShaderSignature();
			pRenderer->SetConstant4x4f("worldViewProj", wvp);
			pRenderer->SetConstantStruct("light", &lightData);
			pRenderer->Apply();
			pRenderer->DrawIndexed();
		}
	}
#endif

	// SPOT LIGHTS
#if 0
	pRenderer->SetShader(SHADERS::DEFERRED_BRDF);
	pRenderer->SetConstant3f("cameraPos", m_pCamera->GetPositionF());

	pRenderer->SetVertexBuffer(IABuffersQuad.first);
	pRenderer->SetIndexBuffer(IABuffersQuad.second);
	
	// for spot lights
	
	pRenderer->Apply();
	pRenderer->DrawIndexed();
#endif

#else
	pRenderer->SetShader(lightingShader);
	ENGINE->SendLightData();

	pRenderer->SetConstant4x4f("matView", sceneView.view);
	pRenderer->SetConstant4x4f("matViewToWorld", sceneView.viewInverse);
	pRenderer->SetConstant4x4f("matPorjInverse", sceneView.projectionInverse);
	//pRenderer->SetSamplerState("sNearestSampler", EDefaultSamplerState::POINT_SAMPLER);
	pRenderer->SetSamplerState("sLinearSampler", EDefaultSamplerState::LINEAR_FILTER_SAMPLER);
	pRenderer->SetSamplerState("sShadowSampler", EDefaultSamplerState::LINEAR_FILTER_SAMPLER_WRAP_UVW);
	pRenderer->SetTexture("texDiffuseRoughnessMap", texDiffuseRoughness);
	pRenderer->SetTexture("texSpecularMetalnessMap", texSpecularMetallic);
	pRenderer->SetTexture("texNormals", texNormal);
	pRenderer->SetTexture("texDepth", depthTexture);
	pRenderer->SetVertexBuffer(IABuffersQuad.first);
	pRenderer->SetIndexBuffer(IABuffersQuad.second);
	pRenderer->Apply();
	pRenderer->DrawIndexed();
#endif	// light volumes

	pRenderer->SetBlendState(EDefaultBlendState::DISABLED);
}

void DebugPass::Initialize(Renderer * pRenderer)
{
	_scissorsRasterizer = pRenderer->AddRasterizerState(ERasterizerCullMode::BACK, ERasterizerFillMode::SOLID, false, true);
}

constexpr size_t SSAO_SAMPLE_KERNEL_SIZE = 64;
TextureID AmbientOcclusionPass::whiteTexture4x4 = -1;
void AmbientOcclusionPass::Initialize(Renderer * pRenderer)
{
	// CREATE SAMPLE KERNEL
	//--------------------------------------------------------------------
	constexpr size_t NOISE_KERNEL_SIZE = 4;

	// src: https://john-chapman-graphics.blogspot.nl/2013/01/ssao-tutorial.html
	for (size_t i = 0; i < SSAO_SAMPLE_KERNEL_SIZE; i++)
	{
		// get a random direction in tangent space, Z-up.
		// As the sample kernel will be oriented along the surface normal, 
		// the resulting sample vectors will all end up in the hemisphere.
		vec3 sample
		(
			RandF(-1, 1),
			RandF(-1, 1),
			RandF(0, 1)	// hemisphere normal direction (up)
		);
		sample.normalize();				// bring the sample to the hemisphere surface
		sample = sample * RandF(0.1f, 1);	// scale to distribute samples within the hemisphere

		// scale vectors with a power curve based on i to make samples close to center of the
		// hemisphere more significant. think of it as i selects where we sample the hemisphere
		// from, which starts from outer region of the hemisphere and as it increases, we 
		// sample closer to the normal direction. 
		float scale = static_cast<float>(i) / SSAO_SAMPLE_KERNEL_SIZE;
		scale = lerp(0.1f, 1.0f, scale * scale);
		sample = sample * scale;

		this->sampleKernel.push_back(sample);
	}

	// CREATE NOISE TEXTURE & SAMPLER
	//--------------------------------------------------------------------
	for (size_t i = 0; i < NOISE_KERNEL_SIZE * NOISE_KERNEL_SIZE; i++)
	{	// create a square noise texture using random directions
		vec3 noise
		(
			RandF(-1, 1),
			RandF(-1, 1),
			0 // noise rotates the kernel around z-axis
		);
		this->noiseKernel.push_back(vec4(noise.normalized()));
	}
	TextureDesc texDesc = {};
	texDesc.width = NOISE_KERNEL_SIZE;
	texDesc.height = NOISE_KERNEL_SIZE;
	texDesc.format = EImageFormat::RGBA32F;
	texDesc.texFileName = "noiseKernel";
	texDesc.pData = this->noiseKernel.data();
	texDesc.dataPitch = sizeof(vec4) * NOISE_KERNEL_SIZE;
	this->noiseTexture = pRenderer->CreateTexture2D(texDesc);

	const float whiteValue = 1.0f;
	std::vector<vec4> white4x4 = std::vector<vec4>(16, vec4(whiteValue, whiteValue, whiteValue, 1));
	texDesc.width = texDesc.height = 4;
	texDesc.texFileName = "white4x4";
	texDesc.pData = white4x4.data();
	this->whiteTexture4x4 = pRenderer->CreateTexture2D(texDesc);

	// The tiling of the texture causes the orientation of the kernel to be repeated and 
	// introduces regularity into the result. By keeping the texture size small we can make 
	// this regularity occur at a high frequency, which can then be removed with a blur step 
	// that preserves the low-frequency detail of the image.
	D3D11_SAMPLER_DESC noiseSamplerDesc = {};
	noiseSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	noiseSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	noiseSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	noiseSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	this->noiseSampler = pRenderer->CreateSamplerState(noiseSamplerDesc);

	// RENDER TARGET
	//--------------------------------------------------------------------
	const EImageFormat imageFormat = R32F;
	RenderTargetDesc rtDesc = {};
	rtDesc.textureDesc.width = pRenderer->WindowWidth();
	rtDesc.textureDesc.height = pRenderer->WindowHeight();
	rtDesc.textureDesc.mipCount = 1;
	rtDesc.textureDesc.arraySize = 1;
	rtDesc.textureDesc.format = imageFormat;
	rtDesc.textureDesc.usage = ETextureUsage::RENDER_TARGET_RW;
	rtDesc.format = imageFormat;

	this->occlusionRenderTarget = pRenderer->AddRenderTarget(rtDesc);
	this->blurRenderTarget		= pRenderer->AddRenderTarget(rtDesc);

	// SHADER
	//--------------------------------------------------------------------
	const ShaderDesc ssaoShaderDesc = 
	{	"SSAO_Shader",
		ShaderStageDesc{ "FullscreenQuad_vs.hlsl", {} },
		ShaderStageDesc{ "SSAO_ps.hlsl", {} },
	};
	this->SSAOShader = pRenderer->CreateShader(ssaoShaderDesc);
#if SSAO_DEBUGGING
	this->radius = 6.5f;
	this->intensity = 1.0f;
#endif
}
//constexpr size_t sz = sizeof(SSAOConstants);
constexpr size_t VEC_SZ = 4;
struct SSAOConstants
{
	XMFLOAT4X4 matProjection;
	//-----------------------------
	XMFLOAT4X4 matProjectionInverse;
	//-----------------------------
	vec2 screenSize;
	float radius;
	float intensity;
	//-----------------------------
	std::array<float, SSAO_SAMPLE_KERNEL_SIZE * VEC_SZ> samples;
};
void AmbientOcclusionPass::RenderOcclusion(Renderer* pRenderer, const TextureID texNormals, const SceneView& sceneView)
{
	const TextureID depthTexture = pRenderer->GetDepthTargetTexture(ENGINE->GetWorldDepthTarget());
	const auto IABuffersQuad = ENGINE->GetGeometryVertexAndIndexBuffers(EGeometry::FULLSCREENQUAD);
	
	XMFLOAT4X4 proj = {};
	XMStoreFloat4x4(&proj, sceneView.projection);

	XMFLOAT4X4 projInv = {};
	XMStoreFloat4x4(&projInv, sceneView.projectionInverse);

	std::array<float, SSAO_SAMPLE_KERNEL_SIZE * VEC_SZ> samples;
	size_t idx = 0;
	for (const vec3& v : sampleKernel)
	{
		samples[idx + 0] = v.x();
		samples[idx + 1] = v.y();
		samples[idx + 2] = v.z();
		samples[idx + 3] = -1.0f;
		idx += VEC_SZ;
	}

	SSAOConstants ssaoConsts = {
		proj,
		projInv,
		pRenderer->GetWindowDimensionsAsFloat2(),
#if SSAO_DEBUGGING
		this->radius,
		this->intensity,
#else
		sceneView.sceneRenderSettings.ssao.radius,
		sceneView.sceneRenderSettings.ssao.intensity,
#endif
		samples
	};

	pRenderer->BeginEvent("Occlusion Pass");

	pRenderer->SetShader(SSAOShader);
	pRenderer->BindRenderTarget(this->occlusionRenderTarget);
	pRenderer->UnbindDepthTarget();
	pRenderer->SetDepthStencilState(EDefaultDepthStencilState::DEPTH_STENCIL_DISABLED);
	pRenderer->SetSamplerState("sNoiseSampler", this->noiseSampler);
	pRenderer->SetSamplerState("sPointSampler", EDefaultSamplerState::POINT_SAMPLER);
	//pRenderer->SetSamplerState("sLinearSampler", EDefaultSamplerState::LINEAR_FILTER_SAMPLER);
	pRenderer->SetTexture("texViewSpaceNormals", texNormals);
	pRenderer->SetTexture("texNoise", this->noiseTexture);
	pRenderer->SetTexture("texDepth", depthTexture);
	pRenderer->SetConstantStruct("SSAO_constants", &ssaoConsts);
	pRenderer->SetVertexBuffer(IABuffersQuad.first);
	pRenderer->SetIndexBuffer(IABuffersQuad.second);
	pRenderer->Apply();
	pRenderer->DrawIndexed();
	
	pRenderer->EndEvent();
}

void AmbientOcclusionPass::BilateralBlurPass(Renderer * pRenderer)
{
	pRenderer->UnbindRenderTargets();
	return;
	pRenderer->BeginEvent("Blur Pass");
	pRenderer->SetShader(bilateralBlurShader);
	
	//pRenderer->BindRenderTarget(this->renderTarget);

	const auto IABuffersQuad = ENGINE->GetGeometryVertexAndIndexBuffers(EGeometry::FULLSCREENQUAD);
	pRenderer->SetVertexBuffer(IABuffersQuad.first);
	pRenderer->SetIndexBuffer(IABuffersQuad.second);
	pRenderer->Apply();
	pRenderer->DrawIndexed();

	pRenderer->EndEvent();
}

void AmbientOcclusionPass::GaussianBlurPass(Renderer * pRenderer)
{
	const TextureID texOcclusion = pRenderer->GetRenderTargetTexture(this->occlusionRenderTarget);
	const vec2 texDimensions = pRenderer->GetWindowDimensionsAsFloat2();
	const auto IABuffersQuad = ENGINE->GetGeometryVertexAndIndexBuffers(EGeometry::FULLSCREENQUAD);

	pRenderer->BeginEvent("Blur Pass");

	pRenderer->SetShader(EShaders::GAUSSIAN_BLUR_4x4);
	pRenderer->BindRenderTarget(this->blurRenderTarget);
	pRenderer->SetTexture("tOcclusion", texOcclusion);
	pRenderer->SetSamplerState("BlurSampler", EDefaultSamplerState::POINT_SAMPLER);
	pRenderer->SetConstant2f("inputTextureDimensions", texDimensions);
	pRenderer->SetVertexBuffer(IABuffersQuad.first);
	pRenderer->SetIndexBuffer(IABuffersQuad.second);
	pRenderer->Apply();
	pRenderer->DrawIndexed();

	pRenderer->EndEvent();
}

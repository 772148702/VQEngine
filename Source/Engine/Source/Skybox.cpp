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

#include "Skybox.h"
#include "Engine.h"
#include "Application/Application.h"
#include "Renderer/Renderer.h"
#include "Utilities/Log.h"

// SKYBOX PRESETS W/ CUBEMAP / ENVIRONMENT MAP
//==========================================================================================================
using FilePaths = std::vector<std::string>;
const  FilePaths s_filePaths = []{
	// cube face order: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476906(v=vs.85).aspx
	//------------------------------------------------------------------------------------------------------
	// 0: RIGHT		1: LEFT
	// 2: UP		3: DOWN
	// 4: FRONT		5: BACK
	//------------------------------------------------------------------------------------------------------

	FilePaths paths(ECubeMapPresets::CUBEMAP_PRESET_COUNT * 6);	// use as an array to access using enum
	
	// night sky by: Hazel Whorley
	paths[ECubeMapPresets::NIGHT_SKY + 0] = "EnvironmentMaps/night_sky/nightsky_rt.png";
	paths[ECubeMapPresets::NIGHT_SKY + 1] = "EnvironmentMaps/night_sky/nightsky_lf.png";
	paths[ECubeMapPresets::NIGHT_SKY + 2] = "EnvironmentMaps/night_sky/nightsky_up.png";
	paths[ECubeMapPresets::NIGHT_SKY + 3] = "EnvironmentMaps/night_sky/nightsky_dn.png";
	paths[ECubeMapPresets::NIGHT_SKY + 4] = "EnvironmentMaps/night_sky/nightsky_ft.png";
	paths[ECubeMapPresets::NIGHT_SKY + 5] = "EnvironmentMaps/night_sky/nightsky_bk.png";

	// other cubemap presets
	// ...
	return paths;
}();

std::vector<Skybox> Skybox::s_Presets(ECubeMapPresets::CUBEMAP_PRESET_COUNT + EEnvironmentMapPresets::ENVIRONMENT_MAP_PRESET_COUNT);
std::pair < std::string, EnvironmentMapFileNames>  GetsIBLFiles(EEnvironmentMapPresets preset)
{
	const std::string sIBLDirectory = Renderer::sHDRTextureRoot + std::string("sIBL/");
	EnvironmentMapFileNames files;
	std::string root;
	switch (preset)
	{
	case MILKYWAY:
		root = sIBLDirectory + "Milkyway/";
		files = {
			"Milkyway_BG.jpg",
			"Milkyway_Light.hdr",
			"Milkyway_small.hdr",
		};
		break;
	case BARCELONA:
		root = sIBLDirectory + "Barcelona_Rooftops/";
		files = {
			"Barce_Rooftop_C_8k.jpg",
			"Barce_Rooftop_C_Env.hdr",
			"Barce_Rooftop_C_3k.hdr",
		};
		break;
	case TROPICAL_BEACH:
		root = sIBLDirectory + "Tropical_Beach/";
		files = {
			"Tropical_Beach_8k.jpg",
			"Tropical_Beach_Env.hdr",
			"Tropical_Beach_3k.hdr",
		};
		break;
	case TROPICAL_RUINS:
		root = sIBLDirectory + "Tropical_Ruins/";
		files = {
			"TropicalRuins_8k.jpg",
			"TropicalRuins_Env.hdr",
			"TropicalRuins_3k.hdr",
		};
		break;
	case WALK_OF_FAME:
		root = sIBLDirectory + "Walk_Of_Fame/";
		files = {
			"Mans_Outside_8k_TMap.jpg",
			"Mans_Outside_Env.hdr",
			"Mans_Outside_2k.hdr",
		};
		break;
	default:
		Log::Warning("Unknown Environment Map Preset");
		break;
	}
	return std::make_pair(root, files);
}

void Skybox::InitializePresets_Async(Renderer* pRenderer, const Settings::Rendering& renderSettings)
{
	EnvironmentMap::Initialize(pRenderer);
	{
		std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
		EnvironmentMap::LoadShaders();
	}
	{
		std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
		EnvironmentMap::CalculateBRDFIntegralLUT("");
	}

	// Cubemap Skyboxes
	//------------------------------------------------------------------------------------------------------------------------------------
	{	// NIGHTSKY		
		// #AsyncLoad: Mutex DEVICE

		const bool bEquirectangular = false;
		const auto offsetIter = s_filePaths.begin() + ECubeMapPresets::NIGHT_SKY;
		const FilePaths filePaths = FilePaths(offsetIter, offsetIter + 6);

		TextureID skydomeTex = -1;
		{
			std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
			skydomeTex = pRenderer->CreateCubemapTexture(filePaths);
		}
		s_Presets[ECubeMapPresets::NIGHT_SKY] = Skybox(pRenderer, skydomeTex, bEquirectangular);
	}

	if (renderSettings.bEnableEnvironmentLighting)
	{
		// HDR / IBL - Equirectangular Skyboxes
		//------------------------------------------------------------------------------------------------------------------------------------
		//EnvironmentMap::Initialize(pRenderer);
		const std::string sIBLDirectory = Renderer::sHDRTextureRoot + std::string("sIBL/");
		const bool bEquirectangular = true;

		EnvironmentMapFileNames files;

		const std::vector<EEnvironmentMapPresets> presets =
		{
			EEnvironmentMapPresets::BARCELONA     ,
			EEnvironmentMapPresets::TROPICAL_BEACH,
			EEnvironmentMapPresets::MILKYWAY	  ,
			EEnvironmentMapPresets::TROPICAL_RUINS,
			EEnvironmentMapPresets::WALK_OF_FAME
		};

		std::for_each(RANGE(presets), [&](auto preset)
		{
			const auto rootAndFilesPair = GetsIBLFiles(preset);
			{
				s_Presets[preset] = Skybox(pRenderer, bEquirectangular);
				s_Presets[preset].Initialize(rootAndFilesPair.second, rootAndFilesPair.first);
			}
		});
	}
}

void Skybox::InitializePresets(Renderer* pRenderer, const Settings::Rendering& renderSettings)
{
	EnvironmentMap::Initialize(pRenderer);
	EnvironmentMap::LoadShaders();
	
	const std::string BRDFLUTTextureFileName = "BRDFIntegrationLUT.png";
	const std::string BRDFLUTTextureFilePath = EnvironmentMap::sTextureCacheDirectory + BRDFLUTTextureFileName;
	if(DirectoryUtil::FileExists(BRDFLUTTextureFilePath) && false)
	{ 
		EnvironmentMap::sBRDFIntegrationLUTTexture = pRenderer->CreateTextureFromFile(BRDFLUTTextureFileName, EnvironmentMap::sTextureCacheDirectory);
	}
	else
	{
		EnvironmentMap::sBRDFIntegrationLUTTexture = EnvironmentMap::CalculateBRDFIntegralLUT(BRDFLUTTextureFilePath);
		pRenderer->SaveTextureToDisk(EnvironmentMap::sBRDFIntegrationLUTTexture, BRDFLUTTextureFilePath);
		// todo: we can unload shaders / render targets here
	}


	// Cubemap Skyboxes
	//------------------------------------------------------------------------------------------------------------------------------------
	{	// NIGHTSKY		
		// #AsyncLoad: Mutex DEVICE

		const bool bEquirectangular = false;
		const auto offsetIter = s_filePaths.begin() + ECubeMapPresets::NIGHT_SKY;
		const FilePaths filePaths = FilePaths(offsetIter, offsetIter + 6);
		
		TextureID skydomeTex = pRenderer->CreateCubemapTexture(filePaths);
		//Skybox::SetPreset(ECubeMapPresets::NIGHT_SKY, std::move(Skybox(pRenderer, skydomeTex, bEquirectangular)));
		s_Presets[ECubeMapPresets::NIGHT_SKY] = Skybox(pRenderer, skydomeTex, bEquirectangular);
	}
	
	if (renderSettings.bEnableEnvironmentLighting)
	{
		// HDR / IBL - Equirectangular Skyboxes
		//------------------------------------------------------------------------------------------------------------------------------------
		//EnvironmentMap::Initialize(pRenderer);
		
		const bool bEquirectangular = true;

		EnvironmentMapFileNames files;

		const std::vector<EEnvironmentMapPresets> presets = 
		{
			EEnvironmentMapPresets::BARCELONA     ,
			EEnvironmentMapPresets::TROPICAL_BEACH,
			EEnvironmentMapPresets::MILKYWAY	  ,
			EEnvironmentMapPresets::TROPICAL_RUINS,
			EEnvironmentMapPresets::WALK_OF_FAME
		};
		std::for_each(RANGE(presets), [&](auto preset)
		{
			const auto rootAndFilesPair = GetsIBLFiles(preset);
			s_Presets[preset] = Skybox(pRenderer, bEquirectangular);
			s_Presets[preset].Initialize(rootAndFilesPair.second, rootAndFilesPair.first);
		});
	}
}


// SKYBOX
//==========================================================================================================
Skybox::Skybox(Renderer* renderer, bool bEquirectangular)
	:
	pRenderer(renderer),
	environmentMap(EnvironmentMap()),
	skyboxTexture(-1),
	skyboxShader(bEquirectangular ? EShaders::SKYBOX_EQUIRECTANGULAR : EShaders::SKYBOX)
{}

Skybox::Skybox(Renderer * renderer, const TextureID skydomeTexture, bool bEquirectangular)
	:
	pRenderer(renderer),
	environmentMap(EnvironmentMap()),
	skyboxTexture(skydomeTexture),
	skyboxShader(bEquirectangular ? EShaders::SKYBOX_EQUIRECTANGULAR : EShaders::SKYBOX)
{}

Skybox::Skybox()
	:
	pRenderer(nullptr),
	environmentMap(EnvironmentMap()),
	skyboxTexture(-1),
	skyboxShader(EShaders::SHADER_COUNT)
{}

bool Skybox::Initialize(const EnvironmentMapFileNames& environmentMapFiles, const std::string& rootDirectory)
{
	environmentMap.Initialize(pRenderer, environmentMapFiles, rootDirectory);
	{
		std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
		skyboxTexture = pRenderer->CreateTextureFromFile(environmentMapFiles.skyboxFileName, rootDirectory);
	}
	return skyboxTexture != -1;
}

// todo: try to remove this dependency
#include "Engine.h"	
void Skybox::Render(const XMMATRIX& viewProj) const
{
	const XMMATRIX& wvp = viewProj;
	const auto IABuffers = ENGINE->GetGeometryVertexAndIndexBuffers(EGeometry::CUBE);

	pRenderer->BeginEvent("Skybox Pass");
	pRenderer->SetShader(skyboxShader);
	pRenderer->SetDepthStencilState(EDefaultDepthStencilState::DEPTH_TEST_ONLY);
	pRenderer->SetRasterizerState(EDefaultRasterizerState::CULL_NONE);
	pRenderer->SetConstant4x4f("worldViewProj", wvp);
	pRenderer->SetTexture("texSkybox", skyboxTexture);
	//pRenderer->SetSamplerState("samWrap", EDefaultSamplerState::WRAP_SAMPLER);
	pRenderer->SetSamplerState("samWrap", EDefaultSamplerState::LINEAR_FILTER_SAMPLER_WRAP_UVW);
	pRenderer->SetVertexBuffer(IABuffers.first);
	pRenderer->SetIndexBuffer(IABuffers.second);
	pRenderer->Apply();
	pRenderer->DrawIndexed();
	pRenderer->EndEvent();
}


// ENVIRONMENT MAP
//==========================================================================================================

// Static Variables - BRDF LUT & Pre-filtered Environment Map
//---------------------------------------------------------------
RenderTargetID EnvironmentMap::sBRDFIntegrationLUTRT = -1;
TextureID EnvironmentMap::sBRDFIntegrationLUTTexture = -1;
ShaderID EnvironmentMap::sBRDFIntegrationLUTShader   = -1;
ShaderID EnvironmentMap::sPrefilterShader = -1;
ShaderID EnvironmentMap::sRenderIntoCubemapShader = -1;
Renderer* EnvironmentMap::spRenderer = nullptr;
std::string EnvironmentMap::sTextureCacheDirectory = "";
//---------------------------------------------------------------

EnvironmentMap::EnvironmentMap() : irradianceMap(-1), environmentMap(-1) {}


void EnvironmentMap::Initialize(Renderer * pRenderer, const EnvironmentMapFileNames & files, const std::string & rootDirectory)
{
	Log::Info("Loading Environment Map: %s", StrUtil::split(rootDirectory, '/').back().c_str());
	{
		std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
		irradianceMap = pRenderer->CreateHDRTexture(files.irradianceMapFileName, rootDirectory);
	}
	{
		std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
		environmentMap = pRenderer->CreateHDRTexture(files.environmentMapFileName, rootDirectory);
	}
	InitializePrefilteredEnvironmentMap(pRenderer->GetTextureObject(environmentMap), pRenderer->GetTextureObject(irradianceMap));
}

void EnvironmentMap::Initialize(Renderer * pRenderer)
{
	spRenderer = pRenderer;
	// create texture cache if it doesn't already exist
	EnvironmentMap::sTextureCacheDirectory = Application::s_WorkspaceDirectory + "/TextureCache/";
	DirectoryUtil::CreateFolderIfItDoesntExist(EnvironmentMap::sTextureCacheDirectory);
	DirectoryUtil::CreateFolderIfItDoesntExist(EnvironmentMap::sTextureCacheDirectory + "/sIBL/");
}

void EnvironmentMap::LoadShaders()
{
	// todo: layouts from reflection?
	const std::vector<InputLayout> layout = {
		{ "POSITION",	FLOAT32_3 },
		{ "NORMAL",		FLOAT32_3 },
		{ "TANGENT",	FLOAT32_3 },
		{ "TEXCOORD",	FLOAT32_2 },
	};

	const std::vector<EShaderType> VS_PS = { EShaderType::VS, EShaderType::PS };

	const std::vector<std::string> IBLConvolution = { "Skybox_vs", "PreFilterConvolution_ps" };		// compute?
	const std::vector<std::string> RenderIntoCubemap = { "Skybox_vs", "RenderIntoCubemap_ps" };
	const std::vector<std::string> BRDFIntegrator = { "FullscreenQuad_vs", "IntegrateBRDF_IBL_ps" };// compute?

	// #AsyncLoad: Mutex DEVICE
	sBRDFIntegrationLUTShader = spRenderer->AddShader("BRDFIntegrator", BRDFIntegrator, VS_PS, layout);
	sPrefilterShader = spRenderer->AddShader("PreFilterConvolution", IBLConvolution, VS_PS, layout);
	sRenderIntoCubemapShader = spRenderer->AddShader("RenderIntoCubemap", RenderIntoCubemap, VS_PS, layout);
}

TextureID EnvironmentMap::CalculateBRDFIntegralLUT(const std::string& outCachedTexturePath)
{
	DXGI_FORMAT format = false ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT;
	constexpr int TEXTURE_DIMENSION = 2048;
	

	// create the render target
	RenderTargetDesc rtDesc = {};
	rtDesc.format = RGBA32F;
	rtDesc.textureDesc.width = TEXTURE_DIMENSION;
	rtDesc.textureDesc.height = TEXTURE_DIMENSION;
	rtDesc.textureDesc.mipCount = 1;
	rtDesc.textureDesc.arraySize = 1;
	rtDesc.textureDesc.format = RGBA32F;
	rtDesc.textureDesc.usage = ETextureUsage::RENDER_TARGET_RW;
	rtDesc.textureDesc.bGenerateMips = false;
	rtDesc.textureDesc.cpuAccessMode = ECPUAccess::CPU_R;
	sBRDFIntegrationLUTRT = spRenderer->AddRenderTarget(rtDesc);


	// TODO: currently, we need to load the shader for not breaking the
	// hardcoded enum order... after shader refactoring, this shouldn't be 
	// an issue and we can uncommend the following section below.
	//
	// create the LUT calculation shader
	//const std::vector<InputLayout> layout = {	// todo: layouts from reflection?
	//	{ "POSITION",	FLOAT32_3 },
	//	{ "NORMAL",		FLOAT32_3 },
	//	{ "TANGENT",	FLOAT32_3 },
	//	{ "TEXCOORD",	FLOAT32_2 },
	//};
	//const std::vector<EShaderType> VS_PS = { EShaderType::VS, EShaderType::PS };
	//const std::vector<std::string> BRDFIntegrator = { "FullscreenQuad_vs", "IntegrateBRDF_IBL_ps" };// compute?
	//sBRDFIntegrationLUTShader = spRenderer->AddShader("BRDFIntegrator", BRDFIntegrator, VS_PS, layout);


	// render the lookup table
	const auto IABuffers = ENGINE->GetGeometryVertexAndIndexBuffers(EGeometry::QUAD);
	spRenderer->BindRenderTarget(sBRDFIntegrationLUTRT);
	spRenderer->UnbindDepthTarget();
	spRenderer->SetShader(sBRDFIntegrationLUTShader);
	spRenderer->SetViewport(2048, 2048);
	spRenderer->SetVertexBuffer(IABuffers.first);
	spRenderer->SetIndexBuffer(IABuffers.second);
	spRenderer->Apply();
	spRenderer->DrawIndexed();
	spRenderer->m_deviceContext->Flush();
	return spRenderer->GetRenderTargetTexture(sBRDFIntegrationLUTRT);
}

TextureID EnvironmentMap::InitializePrefilteredEnvironmentMap(const Texture& environmentMap, const Texture& irradienceMap)
{
	Renderer*& pRenderer = spRenderer;
	const TextureID envMap = environmentMap._id;

	const float screenAspect = 1;
	const float screenNear = 0.01f;
	const float screenFar = 10.0f;
	const float fovy = 90.0f * DEG2RAD;
	const XMMATRIX proj = XMMatrixPerspectiveFovLH(fovy, screenAspect, screenNear, screenFar);

	// cubemap look and up directions for left-handed view
	const XMVECTOR lookDirs[6] = {
		vec3::Right, vec3::Left,
		vec3::Up, vec3::Down,
		vec3::Forward, vec3::Back
	};

	const XMVECTOR upDirs[6] = {
		vec3::Up, vec3::Up,
		vec3::Back, vec3::Forward,
		vec3::Up, vec3::Up
	};

	// create environment map cubemap texture
	const unsigned cubemapDimension = environmentMap._height / 2;
	const unsigned textureSize[2] = { cubemapDimension, cubemapDimension };
	TextureDesc texDesc = {};
	texDesc.width = cubemapDimension;
	texDesc.height = cubemapDimension;
	texDesc.format = EImageFormat::RGBA32F;
	texDesc.texFileName = environmentMap._name + "_preFiltered";
	texDesc.pData = nullptr; // no CPU data
	texDesc.mipCount = PREFILTER_MIP_LEVEL_COUNT;
	texDesc.usage = RENDER_TARGET_RW;
	texDesc.bIsCubeMap = true;
	{
		std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
		prefilteredEnvironmentMap = pRenderer->CreateTexture2D(texDesc);
	}

	texDesc.bGenerateMips = true;
	texDesc.mipCount = PREFILTER_MIP_LEVEL_COUNT;
	{
		std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
		mippedEnvironmentCubemap = pRenderer->CreateTexture2D(texDesc);
	}

	const Texture& prefilteredEnvMapTex = pRenderer->GetTextureObject(prefilteredEnvironmentMap);
	const Texture& mippedEnvironmentCubemapTex = pRenderer->GetTextureObject(mippedEnvironmentCubemap);

	D3D11_TEXTURE2D_DESC desc = {};
	prefilteredEnvMapTex._tex2D->GetDesc(&desc);

	Viewport viewPort = {};
	viewPort.TopLeftX = viewPort.TopLeftY = 0;
	viewPort.MaxDepth = 1.0f;
	viewPort.MinDepth = 0.0f;

	const auto IABuffers = ENGINE->GetGeometryVertexAndIndexBuffers(EGeometry::CUBE);

	auto SetPreFilterStates = [&]()
	{
		pRenderer->UnbindDepthTarget();
		pRenderer->SetShader(sRenderIntoCubemapShader);
		pRenderer->SetDepthStencilState(EDefaultDepthStencilState::DEPTH_STENCIL_DISABLED);
		pRenderer->SetVertexBuffer(IABuffers.first);
		pRenderer->SetIndexBuffer(IABuffers.second);
		pRenderer->SetTexture("tEnvironmentMap", envMap);
		pRenderer->SetSamplerState("sLinear", EDefaultSamplerState::LINEAR_FILTER_SAMPLER_WRAP_UVW);

		viewPort.Width = static_cast<float>(textureSize[0]);
		viewPort.Height = static_cast<float>(textureSize[1]);
		pRenderer->SetViewport(viewPort);
	};

	// RENDER INTO CUBEMAP PASS
	// render cubemap version of the environment map, generate mips, and then bind to pre-filter stage 
	// cube face order: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476906(v=vs.85).aspx
	//------------------------------------------------------------------------------------------------------
	// 0: RIGHT		1: LEFT
	// 2: UP		3: DOWN
	// 4: FRONT		5: BACK
	//------------------------------------------------------------------------------------------------------
	// TODO: Compute shader in single pass.
	{
		std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
		SetPreFilterStates();
		for (unsigned cubeFace = 0; cubeFace < 6; ++cubeFace)
		{
			D3D11_RENDER_TARGET_VIEW_DESC rtDesc = {};
			rtDesc.Format = desc.Format;
			rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			rtDesc.Texture2DArray.MipSlice = 0;
			rtDesc.Texture2DArray.ArraySize = 6 - cubeFace;
			rtDesc.Texture2DArray.FirstArraySlice = cubeFace;

			const RenderTargetID mipTarget = pRenderer->AddRenderTarget(mippedEnvironmentCubemapTex, rtDesc);	// todo: pool RTs.

			const XMMATRIX view = XMMatrixLookAtLH(vec3::Zero, lookDirs[cubeFace], upDirs[cubeFace]);
			const XMMATRIX viewProj = view * proj;

			pRenderer->BindRenderTarget(mipTarget);
			pRenderer->SetConstant4x4f("worldViewProj", viewProj);
			pRenderer->Apply();
			pRenderer->DrawIndexed();
		}
		pRenderer->m_deviceContext->Flush();
	}
	
	{
		std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
		pRenderer->m_deviceContext->GenerateMips(mippedEnvironmentCubemapTex._srv);
	}

	// PREFILTER PASS
	// pre-filter environment map into each cube face and mip level (~ roughness)
	// TODO: Compute shader in single pass.
	{
		std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
		pRenderer->SetShader(sPrefilterShader);
		pRenderer->SetTexture("tEnvironmentMap", mippedEnvironmentCubemap);
		for (unsigned mipLevel = 0; mipLevel < PREFILTER_MIP_LEVEL_COUNT; ++mipLevel)
		{
			viewPort.Width = static_cast<float>(textureSize[0] >> mipLevel);
			viewPort.Height = static_cast<float>(textureSize[1] >> mipLevel);

			pRenderer->SetConstant1f("roughness", static_cast<float>(mipLevel) / (PREFILTER_MIP_LEVEL_COUNT - 1));
			pRenderer->SetConstant1f("resolution", viewPort.Width);

			// cube face order: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476906(v=vs.85).aspx
			//------------------------------------------------------------------------------------------------------
			// 0: RIGHT		1: LEFT
			// 2: UP		3: DOWN
			// 4: FRONT		5: BACK
			//------------------------------------------------------------------------------------------------------
			for (unsigned cubeFace = 0; cubeFace < 6; ++cubeFace)
			{
				D3D11_RENDER_TARGET_VIEW_DESC rtDesc = {};
				rtDesc.Format = desc.Format;
				rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
				rtDesc.Texture2DArray.MipSlice = mipLevel;
				rtDesc.Texture2DArray.ArraySize = 6 - cubeFace;
				rtDesc.Texture2DArray.FirstArraySlice = cubeFace;

				const RenderTargetID mipTarget = pRenderer->AddRenderTarget(prefilteredEnvMapTex, rtDesc);	// todo: pool RTs.

				const XMMATRIX view = XMMatrixLookAtLH(vec3::Zero, lookDirs[cubeFace], upDirs[cubeFace]);
				const XMMATRIX viewProj = view * proj;

				pRenderer->BindRenderTarget(mipTarget);
				pRenderer->SetViewport(viewPort);
				pRenderer->SetConstant4x4f("worldViewProj", viewProj);
				pRenderer->Apply();
				pRenderer->DrawIndexed();
			}
		}
		pRenderer->m_deviceContext->Flush();
	}

	D3D11_SAMPLER_DESC envMapSamplerDesc = {};
	envMapSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	envMapSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	envMapSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	envMapSamplerDesc.MipLODBias = 0;
	envMapSamplerDesc.MinLOD = 0;
	envMapSamplerDesc.MaxLOD = PREFILTER_MIP_LEVEL_COUNT - 1;
	envMapSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	envMapSamplerDesc.MaxAnisotropy = 1;
	{
		std::unique_lock<std::mutex> lck(Engine::mLoadRenderingMutex);
		this->envMapSampler = spRenderer->CreateSamplerState(envMapSamplerDesc);
	}

	// RENDER IRRADIANCE CUBEMAP PASS
	// TODO: irradiance cubemap
	// TODO: Compute shader in single pass.
	//pRenderer->End();	// present frame or device removed...

	// ID3D11ShaderResourceView* pNull[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	// pRenderer->m_deviceContext->PSSetShaderResources(0, 6, &pNull[0]);
	return prefilteredEnvironmentMap;
}

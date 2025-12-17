#pragma once

#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include <chrono>
#include "DDSTextureLoader.h"
#include "resource.h"
#include "Camera.h"
#include "Structures.h"

//imGui include
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <vector>

#include "GameObject.h"
#define FPS60 1.0f/60.0f 
#define FPS120 1.0f/120.0f 
#define GRAVITYFORCE -9.8f

using namespace DirectX;

class DX11PhysicsFramework
{
private:

	int _WindowWidth = 1280;
	int _WindowHeight = 768;

	HWND _windowHandle;

	ID3D11DeviceContext* _immediateContext = nullptr;
	ID3D11Device* _device;
	IDXGIDevice* _dxgiDevice = nullptr;
	IDXGIFactory2* _dxgiFactory = nullptr;
	ID3D11RenderTargetView* _frameBufferView = nullptr;
	IDXGISwapChain1* _swapChain;
	D3D11_VIEWPORT _viewport;

	ID3D11VertexShader* _vertexShader;
	ID3D11InputLayout* _inputLayout;
	ID3D11PixelShader* _pixelShader;
	ID3D11Buffer* _constantBuffer;
	ID3D11Buffer* _cubeVertexBuffer;
	ID3D11Buffer* _cubeIndexBuffer;

	ID3D11Buffer* _planeVertexBuffer;
	ID3D11Buffer* _planeIndexBuffer;

	ID3D11DepthStencilView* _depthBufferView = nullptr;
	ID3D11Texture2D* _depthStencilBuffer = nullptr;

	ID3D11ShaderResourceView* _StoneTextureRV = nullptr;
	ID3D11ShaderResourceView* _GroundTextureRV = nullptr;
	ID3D11ShaderResourceView* _HerculesTextureRV = nullptr;

	ID3D11SamplerState* _samplerLinear = nullptr;

	Light basicLight;

	MeshData _objMeshData;
	vector<GameObject*> _gameObjects;

	Camera * _camera = nullptr;
	float _cameraOrbitRadius = 7.0f;
	float _cameraOrbitRadiusMin = 2.0f;
	float _cameraOrbitRadiusMax = 50.0f;
	float _cameraOrbitAngleXZ = -90.0f;
	float _cameraSpeed = 2.0f;

	float lastMouseX = 0.0f;
	float lastMouseY = 0.0f;

	ConstantBuffer _cbData;

	ID3D11DepthStencilState* _DSLessEqual;
	ID3D11RasterizerState* _RSCullNone;

	ID3D11RasterizerState* _CWcullModeFill; //Fill
	ID3D11RasterizerState* _CWcullModeWire; //WireFrame
	ID3D11RasterizerState* _CWcullModeNone; //No Culling

	enum CurrentState 
	{
		WIRE,
		FILL
	};
	int CurrentStateInt = 0;

	float accumulator = 0.0f;

	//FPS data
	enum FPSType 
	{
		NATURALFPS,
		SIXTYFPS,
		ONEHUNDREDTWENTYFPS
	};
	int CurrentFPSInt = 1;
	int CurrentFPS;

	vector<double> FPSAverageNums;
	int FPSAverageIndex = 0;

	//timer variables
	LARGE_INTEGER TimerFrequency;
	LARGE_INTEGER prevTime;
	LARGE_INTEGER currentTime;

	//curtain dimesnions
	int NumberVerticiesX = 32;
	int NumberVerticiesY = 32;
	int totalParticles;

	//buffers for cloth
	std::vector<Particle>particles;
	ID3D11Buffer* pParticleBuffer = nullptr;
	std::vector<unsigned int> indices;
	ID3D11Buffer* indexBuffer = nullptr;

	//spring vector
	std::vector<Spring> clothSprings;
	const float structuralSpringConst = 500;
	float shearSpringConst = structuralSpringConst * 0.75;
	float bendSpringConst = structuralSpringConst * 0.1;

	//deleting spring variables
	std::vector<int> particlesToDelete;
	int numberofParticlesDestroyed = 0;

	//check for cloth resize
	bool ClothresizeCheck = false;

private:
	HRESULT CreateWindowHandle(HINSTANCE hInstance, int nCmdShow);
	HRESULT CreateD3DDevice();
	HRESULT CreateSwapChainAndFrameBuffer();
	HRESULT InitShadersAndInputLayout();
	HRESULT InitVertexIndexBuffers();
	HRESULT InitPipelineStates();
	HRESULT InitRunTimeData();

public:
	~DX11PhysicsFramework();

	HRESULT Initialise(HINSTANCE hInstance, int nCmdShow);

	bool HandleKeyboard(MSG msg);
	void ClothUpdate(float deltaTime);
	void Update();
	void Draw();
};


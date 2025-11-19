#include "DX11PhysicsFramework.h"

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//imGui
	extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;

	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

bool DX11PhysicsFramework::HandleKeyboard(MSG msg)
{
	XMFLOAT3 cameraPosition = _camera->GetPosition();

	switch (msg.wParam)
	{
	case VK_RETURN: 
		break;
	}

	return false;
}

HRESULT DX11PhysicsFramework::Initialise(HINSTANCE hInstance, int nShowCmd)
{
	HRESULT hr = S_OK;

	hr = CreateWindowHandle(hInstance, nShowCmd);
	if (FAILED(hr)) { OutputDebugStringA("FAIL: CreateWindowHandle\n"); return hr; }

	hr = CreateD3DDevice();
	if (FAILED(hr)) { OutputDebugStringA("FAIL: CreateD3DDevice\n"); return hr; }

	hr = CreateSwapChainAndFrameBuffer();
	if (FAILED(hr)) { OutputDebugStringA("FAIL: SwapChain\n"); return hr; }

	hr = InitShadersAndInputLayout();
	if (FAILED(hr)) { OutputDebugStringA("FAIL: InitShadersAndInputLayout\n"); return hr; }

	hr = InitVertexIndexBuffers();
	if (FAILED(hr)) { OutputDebugStringA("FAIL: InitVertexIndexBuffers\n"); return hr; }

	hr = InitPipelineStates();
	if (FAILED(hr)) { OutputDebugStringA("FAIL: InitPipelineStates\n"); return hr; }

	hr = InitRunTimeData();
	if (FAILED(hr)) { OutputDebugStringA("FAIL: InitRunTimeData\n"); return hr; }

	return hr;
}

HRESULT DX11PhysicsFramework::CreateWindowHandle(HINSTANCE hInstance, int nCmdShow)
{
	const wchar_t* windowName = L"DX11Framework";

	WNDCLASSW wndClass;
	wndClass.style = 0;
	wndClass.lpfnWndProc = WndProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = 0;
	wndClass.hIcon = 0;
	wndClass.hCursor = 0;
	wndClass.hbrBackground = 0;
	wndClass.lpszMenuName = 0;
	wndClass.lpszClassName = windowName;

	RegisterClassW(&wndClass);

	_windowHandle = CreateWindowExW(0, windowName, windowName, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, _WindowWidth, _WindowHeight, nullptr, nullptr, hInstance, nullptr);

	return S_OK;
}

HRESULT DX11PhysicsFramework::CreateD3DDevice()
{
	HRESULT hr = S_OK;

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
	};

	ID3D11Device* baseDevice;
	ID3D11DeviceContext* baseDeviceContext;

	DWORD createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | createDeviceFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &baseDevice, nullptr, &baseDeviceContext);
	if (FAILED(hr)) return hr;

	///////////////////////////////////////////////////////////////////////////////////////////////

	hr = baseDevice->QueryInterface(__uuidof(ID3D11Device), reinterpret_cast<void**>(&_device));
	hr = baseDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext), reinterpret_cast<void**>(&_immediateContext));

	baseDevice->Release();
	baseDeviceContext->Release();

	///////////////////////////////////////////////////////////////////////////////////////////////

	hr = _device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&_dxgiDevice));
	if (FAILED(hr)) return hr;

	IDXGIAdapter* dxgiAdapter;
	hr = _dxgiDevice->GetAdapter(&dxgiAdapter);
	hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&_dxgiFactory));
	dxgiAdapter->Release();

	return S_OK;
}

HRESULT DX11PhysicsFramework::CreateSwapChainAndFrameBuffer()
{
	HRESULT hr = S_OK;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	swapChainDesc.Width = 0; // Defer to WindowWidth
	swapChainDesc.Height = 0; // Defer to WindowHeight
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //FLIP* modes don't support sRGB backbuffer
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = 0;

	hr = _dxgiFactory->CreateSwapChainForHwnd(_device, _windowHandle, &swapChainDesc, nullptr, nullptr, &_swapChain);
	if (FAILED(hr)) return hr;

	///////////////////////////////////////////////////////////////////////////////////////////////

	ID3D11Texture2D* frameBuffer = nullptr;

	hr = _swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&frameBuffer));
	if (FAILED(hr)) return hr;

	D3D11_TEXTURE2D_DESC depthBufferDesc;
	frameBuffer->GetDesc(&depthBufferDesc);

	D3D11_RENDER_TARGET_VIEW_DESC framebufferDesc = {};
	framebufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //sRGB render target enables hardware gamma correction
	framebufferDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	hr = _device->CreateRenderTargetView(frameBuffer, &framebufferDesc, &_frameBufferView);
	if (FAILED(hr))
	{
		frameBuffer->Release();
		return hr;
	}
	
	frameBuffer->Release();

	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	hr = _device->CreateTexture2D(&depthBufferDesc, nullptr, &_depthStencilBuffer);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = _device->CreateDepthStencilView(_depthStencilBuffer, nullptr, &_depthBufferView);
	if (FAILED(hr))
	{
		return hr;
	}

	return hr;
}

HRESULT DX11PhysicsFramework::InitShadersAndInputLayout()
{

	HRESULT hr = S_OK;
	ID3DBlob* errorBlob;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* vsBlob;

    // Compile the vertex shader
    hr = D3DCompileFromFile(L"SimpleShaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS_main", "vs_5_0", dwShaderFlags, 0, &vsBlob, &errorBlob);
	if (FAILED(hr))
	{
		MessageBoxA(_windowHandle, (char*)errorBlob->GetBufferPointer(), nullptr, ERROR);
		errorBlob->Release();
		return hr;
	}

	// Create the vertex shader
	hr = _device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &_vertexShader);

	if (FAILED(hr))
	{	
		vsBlob->Release();
        return hr;
	}

	// Compile the pixel shader
	ID3DBlob* psBlob;
	hr = D3DCompileFromFile(L"SimpleShaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS_main", "ps_5_0", dwShaderFlags, 0, &psBlob, &errorBlob);
	if (FAILED(hr))
	{
		MessageBoxA(_windowHandle, (char*)errorBlob->GetBufferPointer(), nullptr, ERROR);
		errorBlob->Release();
		return hr;
	}

	// Create the pixel shader
	hr = _device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &_pixelShader);
	
    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
    {
		{ "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,   D3D11_INPUT_PER_VERTEX_DATA, 0 },		 // position
		{ "PREVPOS",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,  D3D11_INPUT_PER_VERTEX_DATA, 0 },			 // PrevPos
		{ "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24,  D3D11_INPUT_PER_VERTEX_DATA, 0 },		 // Normal
		{ "VELOCITY",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36,  D3D11_INPUT_PER_VERTEX_DATA, 0 },		 // Velocity
		{ "ACCUMULATEDFORCE",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48,  D3D11_INPUT_PER_VERTEX_DATA, 0 }, // accumulatedForce
		{ "MASS",  0, DXGI_FORMAT_R32_FLOAT,       0, 60,  D3D11_INPUT_PER_VERTEX_DATA, 0 },			 // mass
		{ "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 64,  D3D11_INPUT_PER_VERTEX_DATA, 0 },		 // TexCoord
	};

    // Create the input layout
	_device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &_inputLayout);

	if (FAILED(hr))
		return hr;

	vsBlob->Release();
	psBlob->Release();
	errorBlob->Release();

	return hr;
}

HRESULT DX11PhysicsFramework::InitVertexIndexBuffers()
{
	HRESULT hr;

    D3D11_BUFFER_DESC bd;

    D3D11_SUBRESOURCE_DATA InitData;

	//get total verticies in the cloth
	totalParticles = NumberVerticiesX * NumberVerticiesY;
	float spacing = 3.0f / NumberVerticiesX;
	int tempindex = 0;

	//define all particles first so spring checks can work
	for (int y = 0; y < NumberVerticiesY; y++)
	{
		for (int x = 0; x < NumberVerticiesX; x++)
		{
			Particle p;
			p.Pos.x = (x - NumberVerticiesX / 2.0f) * spacing;
			p.Pos.y = (y - NumberVerticiesY / 2.0f) * spacing;
			p.Pos.z = 0.0f; //flat cloth on XY plane
			p.Normal = XMFLOAT3(0, 0, 1);
			p.TexC.x = (float)x / (NumberVerticiesX - 1);
			p.TexC.y = (float)y / (NumberVerticiesY - 1);
			particles.push_back(p);
		}
	}

	//make springs
	for (int y = 0; y < NumberVerticiesY; y++)
	{
		for (int x = 0; x < NumberVerticiesX; x++)
		{
			//horizontal adjacent structure spring
			if ((tempindex + 1) % NumberVerticiesX != 0)
			{
				Spring s;
				s.ParticleIndiceA = tempindex;
				s.ParticleIndiceB = tempindex + 1;
				s.springConstant = structuralSpringConst;

				//pythagorous for base length
				s.restLength = sqrt(((particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x) * (particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x)) +
					((particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y) * (particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y)) +
					((particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z) * (particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z)));

				clothSprings.push_back(s);
			}

			//verticle adjacent structure spring
			if (tempindex + NumberVerticiesX < totalParticles)
			{
				Spring s;
				s.ParticleIndiceA = tempindex;
				s.ParticleIndiceB = tempindex + NumberVerticiesX;
				s.springConstant = structuralSpringConst;

				//pythagorous for base length
				s.restLength = sqrt(((particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x) * (particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x)) +
					((particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y) * (particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y)) +
					((particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z) * (particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z)));

				clothSprings.push_back(s);
			}

			//diagonal shear springs
			if ((tempindex + 1) % NumberVerticiesX != 0)
			{
				//diagonal down right
				if (tempindex + 1 + NumberVerticiesX < totalParticles)
				{
					Spring s;
					s.ParticleIndiceA = tempindex;
					s.ParticleIndiceB = tempindex + 1 + NumberVerticiesX;
					s.springConstant = shearSpringConst;

					//pythagorous for base length
					s.restLength = sqrt(((particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x) * (particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x)) +
						((particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y) * (particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y)) +
						((particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z) * (particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z)));

					clothSprings.push_back(s);
				}

				//digonal up right
				if (tempindex + 1 - NumberVerticiesX > 0)
				{
					Spring s;
					s.ParticleIndiceA = tempindex;
					s.ParticleIndiceB = tempindex + 1 - NumberVerticiesX;
					s.springConstant = shearSpringConst;

					//pythagorous for base length
					s.restLength = sqrt(((particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x) * (particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x)) +
						((particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y) * (particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y)) +
						((particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z) * (particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z)));

					clothSprings.push_back(s);
				}
			}

			//neighbours neighbour spring horizontal
			if ((tempindex + 2) % NumberVerticiesX != 0 && (tempindex + 2) % NumberVerticiesX != 1)
			{
				Spring s;
				s.ParticleIndiceA = tempindex;
				s.ParticleIndiceB = tempindex + 2;
				s.springConstant = bendSpringConst;

				//pythagorous for base length
				s.restLength = sqrt(((particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x) * (particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x)) +
					((particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y) * (particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y)) +
					((particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z) * (particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z)));

				clothSprings.push_back(s);
			}

			//neighbours neighbour spring vertical
			if ((tempindex + NumberVerticiesX + NumberVerticiesX) < totalParticles)
			{
				Spring s;
				s.ParticleIndiceA = tempindex;
				s.ParticleIndiceB = tempindex + NumberVerticiesX + NumberVerticiesX;
				s.springConstant = bendSpringConst;

				//pythagorous for base length
				s.restLength = sqrt(((particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x) * (particles[s.ParticleIndiceA].Pos.x - particles[s.ParticleIndiceB].Pos.x)) +
					((particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y) * (particles[s.ParticleIndiceA].Pos.y - particles[s.ParticleIndiceB].Pos.y)) +
					((particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z) * (particles[s.ParticleIndiceA].Pos.z - particles[s.ParticleIndiceB].Pos.z)));

				clothSprings.push_back(s);
			}


			tempindex++;
		}
	}


	//deifne a vertex buffer for the cloth
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof(Particle) * particles.size();
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA vertexData = {};
	vertexData.pSysMem = particles.data();

	hr = _device->CreateBuffer(&bufferDesc, &vertexData, &pParticleBuffer); 

	if (FAILED(hr))
		return hr;

	//define index buffer
	for (int y = 0; y < NumberVerticiesY - 1; y++)
	{
		for (int x = 0; x < NumberVerticiesX - 1; x++)
		{
			//define four points of a quad
			int p0 = y * NumberVerticiesX + x;
			int p1 = y * NumberVerticiesX + (x + 1);
			int p2 = (y + 1) * NumberVerticiesX + x;
			int p3 = (y + 1) * NumberVerticiesX + (x + 1);

			// Triangle 1
			indices.push_back(p0);
			indices.push_back(p2);
			indices.push_back(p1);

			// Triangle 2
			indices.push_back(p2);
			indices.push_back(p3);
			indices.push_back(p1);
		}
	}

	D3D11_BUFFER_DESC ibDesc = {}; 
	ibDesc.Usage = D3D11_USAGE_DEFAULT; 
	ibDesc.ByteWidth = sizeof(unsigned int) * indices.size(); 
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER; 

	D3D11_SUBRESOURCE_DATA initData = {}; 
	initData.pSysMem = indices.data(); 

	hr = _device->CreateBuffer(&ibDesc, &initData, &indexBuffer);  

	if (FAILED(hr))
		return hr;

	return S_OK;
}


HRESULT DX11PhysicsFramework::InitPipelineStates()
{
	HRESULT hr = S_OK;

	_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_immediateContext->IASetInputLayout(_inputLayout);

	// Rasterizer
	D3D11_RASTERIZER_DESC cmdesc;
	ZeroMemory(&cmdesc, sizeof(D3D11_RASTERIZER_DESC));
	cmdesc.FillMode = D3D11_FILL_SOLID;
	cmdesc.CullMode = D3D11_CULL_BACK;

	hr = _device->CreateRasterizerState(&cmdesc, &_CWcullModeFill);
	
	//wire frame Rasterizer
	D3D11_RASTERIZER_DESC cmdesc2;
	ZeroMemory(&cmdesc2, sizeof(D3D11_RASTERIZER_DESC));
	cmdesc2.FillMode = D3D11_FILL_WIREFRAME;
	cmdesc2.CullMode = D3D11_CULL_BACK;

	hr = _device->CreateRasterizerState(&cmdesc2, &_CWcullModeWire);

	//None Rasterizer
	D3D11_RASTERIZER_DESC cmdesc3;
	ZeroMemory(&cmdesc3, sizeof(D3D11_RASTERIZER_DESC));
	cmdesc3.FillMode = D3D11_FILL_SOLID;
	cmdesc3.CullMode = D3D11_CULL_NONE;

	hr = _device->CreateRasterizerState(&cmdesc3, &_CWcullModeNone);

	_immediateContext->RSSetState(_CWcullModeFill);

	D3D11_DEPTH_STENCIL_DESC dssDesc;
	ZeroMemory(&dssDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	dssDesc.DepthEnable = true;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	_device->CreateDepthStencilState(&dssDesc, &_DSLessEqual);

	_immediateContext->OMSetDepthStencilState(_DSLessEqual, 0);

	D3D11_SAMPLER_DESC bilinearSamplerdesc = {};
	bilinearSamplerdesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	bilinearSamplerdesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	bilinearSamplerdesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	bilinearSamplerdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	bilinearSamplerdesc.MaxLOD = INT_MAX;
	bilinearSamplerdesc.MinLOD = 0;

	hr = _device->CreateSamplerState(&bilinearSamplerdesc, &_samplerLinear);
	if (FAILED(hr)) return hr;

    return S_OK;
}

HRESULT DX11PhysicsFramework::InitRunTimeData()
{
	HRESULT hr = S_OK;

	D3D11_BUFFER_DESC constantBufferDesc = {};
	constantBufferDesc.ByteWidth = sizeof(ConstantBuffer);
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	_viewport = { 0.0f, 0.0f, (float)_WindowWidth, (float)_WindowHeight, 0.0f, 1.0f };
	_immediateContext->RSSetViewports(1, &_viewport);

	hr = _device->CreateBuffer(&constantBufferDesc, nullptr, &_constantBuffer);
	if (FAILED(hr)) { return hr; }

	hr = CreateDDSTextureFromFile(_device, L"Resources\\Textures\\stone.dds", nullptr, &_StoneTextureRV);
	hr = CreateDDSTextureFromFile(_device, L"Resources\\Textures\\floor.dds", nullptr, &_GroundTextureRV);
	if (FAILED(hr)) { return hr; }

	// Setup Camera
	XMFLOAT3 eye = XMFLOAT3(0.0f, 0.0f, -6.0f);
	XMFLOAT3 at = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f);

	_camera = new Camera(eye, at, up, (float)_WindowWidth, (float)_WindowHeight, 0.01f, 200.0f);

	// Setup the scene's light
	basicLight.AmbientLight = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	basicLight.DiffuseLight = XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f);
	basicLight.SpecularLight = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	basicLight.SpecularPower = 10.0f;
	basicLight.LightVecW = XMFLOAT3(0.0f, 0.5f, -1.0f);

	Material shinyMaterial;
	shinyMaterial.ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	shinyMaterial.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	shinyMaterial.specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);

	Material noSpecMaterial;
	noSpecMaterial.ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	noSpecMaterial.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	noSpecMaterial.specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	//define gameobjects

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

	//Resize FPS Vector
	FPSAverageNums.resize(10);


	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(_windowHandle);
	ImGui_ImplDX11_Init(_device, _immediateContext);

	return S_OK;
}

DX11PhysicsFramework::~DX11PhysicsFramework()
{
	delete _camera;
	for each (GameObject * go in _gameObjects)
	{
		delete go;
	}

	if (_immediateContext)_immediateContext->Release();

	if (_frameBufferView)_frameBufferView->Release();
	if (_depthBufferView)_depthBufferView->Release();
	if (_depthStencilBuffer)_depthStencilBuffer->Release();
	if (_swapChain)_swapChain->Release();
	if (_CWcullModeFill)_CWcullModeFill->Release();
	if (_CWcullModeWire)_CWcullModeWire->Release();
	if (_vertexShader)_vertexShader->Release();
	if (_inputLayout)_inputLayout->Release();
	if (_pixelShader)_pixelShader->Release();
	if (_constantBuffer)_constantBuffer->Release();
	
	if (_DSLessEqual) _DSLessEqual->Release();
	if (_RSCullNone) _RSCullNone->Release();

	if (_samplerLinear)_samplerLinear->Release();
	if (_StoneTextureRV)_StoneTextureRV->Release();
	if (_GroundTextureRV)_GroundTextureRV->Release();
	if (_HerculesTextureRV)_HerculesTextureRV->Release();

	if (_dxgiDevice)_dxgiDevice->Release();
	if (_dxgiFactory)_dxgiFactory->Release();
	if (_device)_device->Release();

	ImGui_ImplDX11_Shutdown(); 
	ImGui_ImplWin32_Shutdown(); 
	ImGui::DestroyContext(); 
}

void DX11PhysicsFramework::Update()
{
	static auto last = chrono::steady_clock::now();
	auto old = last;
	last = chrono::steady_clock::now();
	const chrono::duration<float> frameTime = last - old;
	float deltaTime = frameTime.count();

	accumulator += deltaTime;

	if (CurrentFPSInt == NATURALFPS) // run at natural FPS
	{
		_camera->Update();

		// Update objects
		for (auto gameObject : _gameObjects)
		{
			gameObject->Update(deltaTime);
		}


		FPSAverageNums[FPSAverageIndex] = 1.0f / deltaTime;
		FPSAverageIndex++;


		//average FPS over 10 frames
		if (FPSAverageIndex == 10)
		{
			FPSAverageIndex = 0;
		}
	}

	else if (CurrentFPSInt == SIXTYFPS)  //run the update loop if on 60FPS
	{
		while (accumulator >= FPS60)
		{
			_camera->Update();

			// Update objects
			for (auto gameObject : _gameObjects)
			{
				gameObject->Update(deltaTime);
			}

			FPSAverageNums[FPSAverageIndex] = 1.0f / accumulator;
			FPSAverageIndex++;

			//average FPS over 10 frames
			if (FPSAverageIndex == 10)
			{
				FPSAverageIndex = 0;
			}

			accumulator -= FPS60;
		}
	}

	else if (CurrentFPSInt == ONEHUNDREDTWENTYFPS)  //run the update lop if on 120FPS
	{
		while (accumulator >= FPS120)
		{
			_camera->Update();

			// Update objects
			for (auto gameObject : _gameObjects)
			{
				gameObject->Update(deltaTime);
			}

			FPSAverageNums[FPSAverageIndex] = 1.0f / accumulator;
			FPSAverageIndex++;


			//average FPS over 10 frames
			if (FPSAverageIndex == 10)
			{
				FPSAverageIndex = 0;
			}


			accumulator -= FPS120;
		}
	}


	double tempFpsTotal = 0;
	for (int i = 0; i < FPSAverageNums.size(); i++)
	{
		tempFpsTotal += FPSAverageNums[i];
	}

	CurrentFPS = tempFpsTotal / FPSAverageNums.size();
}



void DX11PhysicsFramework::Draw()
{
	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame(); 
	ImGui_ImplWin32_NewFrame(); 
	ImGui::NewFrame(); 


    //
    // Clear buffers
    //
	float ClearColor[4] = { 0.25f, 0.25f, 0.75f, 1.0f }; // red,green,blue,alpha
	_immediateContext->OMSetRenderTargets(1, &_frameBufferView, _depthBufferView);
    _immediateContext->ClearRenderTargetView(_frameBufferView, ClearColor);
	_immediateContext->ClearDepthStencilView(_depthBufferView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    //
    // Setup buffers and render scene
    //
	_immediateContext->VSSetShader(_vertexShader, nullptr, 0);
	_immediateContext->PSSetShader(_pixelShader, nullptr, 0);

	_immediateContext->VSSetConstantBuffers(0, 1, &_constantBuffer);
	_immediateContext->PSSetConstantBuffers(0, 1, &_constantBuffer);
	_immediateContext->PSSetSamplers(0, 1, &_samplerLinear);
	_immediateContext->IASetInputLayout(_inputLayout);

	XMFLOAT4X4 tempView = _camera->GetView();
	XMFLOAT4X4 tempProjection = _camera->GetProjection();

	XMMATRIX view = XMLoadFloat4x4(&tempView);
	XMMATRIX projection = XMLoadFloat4x4(&tempProjection);
	
	_cbData.World = XMMatrixTranspose(XMMatrixIdentity());

	_cbData.View = XMMatrixTranspose(view);
	_cbData.Projection = XMMatrixTranspose(projection);
	
	_cbData.light = basicLight;
	_cbData.EyePosW = _camera->GetPosition();

	//draw cloth
	_cbData.HasTexture = 1.0f;
	_immediateContext->PSSetShaderResources(0, 1, &_StoneTextureRV);

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	HRESULT hr = _immediateContext->Map(_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (SUCCEEDED(hr))
	{
		memcpy(mapped.pData, &_cbData, sizeof(_cbData));
		_immediateContext->Unmap(_constantBuffer, 0);
	}

	UINT stride = sizeof(Particle);
	UINT offset = 0;
	_immediateContext->IASetVertexBuffers(0, 1, &pParticleBuffer, &stride, &offset);
	_immediateContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0); 

	_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); 
	_immediateContext->DrawIndexed(static_cast<UINT>(indices.size()), 0, 0);


	//imGui menues
	// Create a window for changing the framerate
	ImGui::Begin("FrameRate Buttons");

	// Display contents in a scrolling region
	ImGui::TextColored(ImVec4(1, 1, 1, 1), "Current FPS rate: %d", CurrentFPS);
	ImGui::TextColored(ImVec4(1, 1, 1, 1), "Click to change the frame rate");
	ImGui::BeginChild("NaturalFPS Button");
	if (ImGui::Button("NaturalFPS", ImVec2(100, 40))) //button to change to 60 FPS
	{
		CurrentFPSInt = NATURALFPS;
		FPSAverageIndex = 0;
	}
	if (ImGui::Button("60FPS", ImVec2(100, 40))) //button to change to 60 FPS
	{
		CurrentFPSInt = SIXTYFPS;
		FPSAverageIndex = 0;
	}
	if (ImGui::Button("120FPS", ImVec2(100, 40))) //button to change to 120 FPS
	{
		CurrentFPSInt = ONEHUNDREDTWENTYFPS;
		FPSAverageIndex = 0;
	}
	ImGui::EndChild();
	ImGui::End();


	ImGui::Begin("Rasterizer Buttons");

	// Display contents in a scrolling region
	ImGui::TextColored(ImVec4(1, 1, 1, 1), "Click to change the cull type");
	ImGui::BeginChild("Wireframe Button");
	if (ImGui::Button("Wire Frame", ImVec2(100, 40)))
	{
		_immediateContext->RSSetState(_CWcullModeWire);
		CurrentStateInt = WIRE;
	}
	if (ImGui::Button("Full Face", ImVec2(100, 40)))
	{
		_immediateContext->RSSetState(_CWcullModeFill);
		CurrentStateInt = FILL;
	}
	ImGui::EndChild();
	ImGui::End();


	ImGui::Begin("Vertex Count Buttons");

	// Display contents in a scrolling region
	//verticies buttons
	ImGui::TextColored(ImVec4(1, 1, 1, 1), "Click to change the amount  of verticies");
	ImGui::BeginChild("verticies buttons");
	if (ImGui::Button("1024 (32 X 32)" , ImVec2(150, 40)))
	{
		NumberVerticiesX = 32;
		NumberVerticiesY = 32;
		particles.clear();
		indices.clear();
		InitVertexIndexBuffers();
	}
	if (ImGui::Button("2048 (64 X 32)", ImVec2(150, 40)))
	{
		NumberVerticiesX = 64;
		NumberVerticiesY = 32;
		particles.clear();
		indices.clear();
		InitVertexIndexBuffers();
	}
	if (ImGui::Button("4096 (64 X 64)", ImVec2(150, 40)))
	{
		NumberVerticiesX = 64;
		NumberVerticiesY = 64;
		particles.clear();
		indices.clear();
		InitVertexIndexBuffers();
	}
	if (ImGui::Button("16,384 (128 X 128)", ImVec2(150, 40)))
	{
		NumberVerticiesX = 128;
		NumberVerticiesY = 128;
		particles.clear();
		indices.clear();
		InitVertexIndexBuffers();
	}
	if (ImGui::Button("32,768 (256 X 128)", ImVec2(150, 40)))
	{
		NumberVerticiesX = 256;
		NumberVerticiesY = 128;
		particles.clear();
		indices.clear();
		InitVertexIndexBuffers();
	}
	if (ImGui::Button("65,536 (256 X 256)", ImVec2(150, 40)))
	{
		NumberVerticiesX = 256;
		NumberVerticiesY = 256;
		particles.clear();
		indices.clear();
		InitVertexIndexBuffers();
	}
	ImGui::EndChild();
	ImGui::End();

	// Rendering imGui
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	
	// 
    // Present our back buffer to our front buffer
    //
    _swapChain->Present(0, 0);
}
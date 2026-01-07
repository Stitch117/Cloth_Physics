#pragma once
#include <cstring>
#include <d3d11.h>
#include <directxmath.h>

using namespace DirectX;

struct SurfaceInfo
{
	XMFLOAT4 AmbientMtrl;
	XMFLOAT4 DiffuseMtrl;
	XMFLOAT4 SpecularMtrl;
};

struct Light
{
	XMFLOAT4 AmbientLight;
	XMFLOAT4 DiffuseLight;
	XMFLOAT4 SpecularLight;

	float SpecularPower;
	XMFLOAT3 LightVecW;
};

struct ConstantBuffer
{
	XMMATRIX World;
	XMMATRIX View;
	XMMATRIX Projection;

	SurfaceInfo surface;
	Light light;

	XMFLOAT3 EyePosW;
	float HasTexture;
};
struct Particle
{
	XMFLOAT3 Pos;
	XMFLOAT3 PrevPos;
	XMFLOAT3 Normal;
	XMFLOAT3 Velocity;
	XMFLOAT3 accumulatedForce;
	float mass;
	XMFLOAT2 TexC;
	int IsPinned;
};

struct ClothSimParams
{
	float dt;
	XMFLOAT3 gravity;
};

struct Spring
{
	int ParticleIndiceA;
	int ParticleIndiceB;
	float springConstant;
	float dampingConstant;
	float restLength;
};

struct MeshData
{
	ID3D11Buffer* VertexBuffer;
	ID3D11Buffer* IndexBuffer;
	UINT VBStride;
	UINT VBOffset;
	UINT IndexCount;
};
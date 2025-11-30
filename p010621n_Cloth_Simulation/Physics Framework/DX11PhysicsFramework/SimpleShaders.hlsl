//--------------------------------------------------------------------------------------
// File: DX11 Framework.hlsl
//--------------------------------------------------------------------------------------

Texture2D txDiffuse : register(t0);

SamplerState samLinear : register(s0);

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------

struct SurfaceInfo
{
	float4 AmbientMtrl;
	float4 DiffuseMtrl;
	float4 SpecularMtrl;
};

struct Light
{
	float4 AmbientLight;
	float4 DiffuseLight;
	float4 SpecularLight;

	float SpecularPower;
	float3 LightVecW;
};

cbuffer ConstantBuffer : register( b0 )
{
	matrix World;
	matrix View;
	matrix Projection;

	SurfaceInfo surface;
	Light light;

	float3 EyePosW;
	float HasTexture;
}

struct VS_INPUT
{
    float3 Pos : POSITION;
    float3 PrevPos : PREVPOS;
    float3 Normal : NORMAL;
    float3 Velocity : VELOCITY;
    float3 AccumulatedForce : ACCUMULATEDFORCE;
    float Mass : MASS;
    float2 Tex : TEXCOORD0;
    bool IsPinned : IsPinned;
};

//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 PosH : SV_POSITION; // final clip-space position
    float3 PosW : POSITION; // world position
    float3 NormW : NORMAL; // world normal
    float3 WorldVelocity : VELOCITY;
    float2 Tex : TEXCOORD0;
};

VS_OUTPUT VS_main(VS_INPUT input)
{
    VS_OUTPUT output;

	//calculate world pos
    float4 posL = float4(input.Pos, 1.0f);
    float4 posW = mul(posL, World);
    output.PosW = posW.xyz;

	//calculate clipspce pos
    float4 posH = mul(posW, View);
    posH = mul(posH, Projection);
    output.PosH = posH; 

    output.NormW = normalize(mul(float4(input.Normal, 0), World).xyz);
    output.WorldVelocity = input.Velocity;
    output.Tex = input.Tex;

    return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS_main(VS_OUTPUT input) : SV_Target
{
	float3 normalW = normalize(input.NormW);

	float3 toEye = normalize(EyePosW - input.PosW);

	// Get texture data from file
	float4 textureColour = txDiffuse.Sample(samLinear, input.Tex);

	float4 ambient = float4(0.0f, 0.0f, 0.0f,0.0f);
    float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 specular = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float3 lightLecNorm = normalize(light.LightVecW);
	// Compute Colour

	// Compute the reflection vector.
	float3 r = reflect(-lightLecNorm, normalW);

	// Determine how much specular light makes it into the eye.
	float specularAmount = pow(saturate(dot(r, toEye)), light.SpecularPower);

	// Determine the diffuse light intensity that strikes the vertex.
    float diffuseAmount = saturate(dot(lightLecNorm, normalW));

	// Only display specular when there is diffuse
	if (diffuseAmount <= 0.0f)
	{
		specularAmount = 0.0f;
	}

	// Sum all the terms together and copy over the diffuse alpha.
	float4 finalColour;

	if (HasTexture == 1.0f)
	{
        specular += specularAmount * (surface.SpecularMtrl * light.SpecularLight);
        diffuse += diffuseAmount * (textureColour * light.DiffuseLight);
        ambient += (textureColour * light.AmbientLight);
		
		finalColour = ambient + diffuse + specular;
    }
	else
	{
        specular += specularAmount * (surface.SpecularMtrl * light.SpecularLight);
        diffuse += diffuseAmount * (surface.DiffuseMtrl * light.DiffuseLight);
        ambient += (surface.AmbientMtrl * light.AmbientLight);
		
		finalColour.rgb = ambient + diffuse + specular;
	}

	finalColour.a = surface.DiffuseMtrl.a;

	return finalColour;
}
struct Particle
{
    float3 Pos;
    float3 PrevPos;
    float3 Normal;
    float3 Velocity;
    float3 accumulatedForce;
    float mass;
    float2 TexC;
    int IsPinned;
};

RWStructuredBuffer<Particle> particles : register(u0);

cbuffer ClothSimParams : register(b0)
{
    float dt;
    float3 gravity;
};

[numthreads(256, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint i = DTid.x;

    Particle p = particles[i];
    
    if (i == 1)
    {
        p.Pos = (9999.0f, 9999.0f, 9999.0f);
    }
    
    
        if (p.IsPinned)
            return;

    // Basic integration (matches your CPU intent)
        p.Velocity += gravity * dt;
        p.PrevPos = p.Pos;
        p.Pos += p.Velocity * dt;

        particles[i] = p;
}

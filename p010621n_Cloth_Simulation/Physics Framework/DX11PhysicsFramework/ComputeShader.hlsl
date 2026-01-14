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

struct Spring
{
    int ParticleIndiceA;
    int ParticleIndiceB;
    float springConstant;
    float dampingConstant;
    float restLength;
};

RWStructuredBuffer<Particle> particles : register(u0);
StructuredBuffer<Spring> clothSprings : register(t0);
RWStructuredBuffer<int3> positionDeltas : register(u1);
RWStructuredBuffer<uint> deltaCounts : register(u2);

static const float FIXED_SCALE = 100000.0f;

cbuffer ClothSimParams : register(b0)
{
    float dt;
    float3 gravity;
    int constraintIterations;
    int numOfSprings;
    float maxStretchLimit;
    int PAD;
};

//integrate positions on 256 threads
[numthreads(1024, 1, 1)]
void CS_Integrate(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;

    Particle p = particles[i];

    if (!p.IsPinned)
    {
        float3 temp = p.Pos;

        float3 vel = p.Pos - p.PrevPos;
        vel += gravity * (dt * dt);

        p.Pos += vel;
        p.PrevPos = temp;
    }

    particles[i] = p;
}


//simulate springs on 256 threads
[numthreads(1024, 1, 1)]
void CS_SolveSprings(uint3 id : SV_DispatchThreadID)
{
    uint s = id.x;
    if (s >= numOfSprings)
        return;

    Spring spring = clothSprings[s];

    uint iA = spring.ParticleIndiceA;
    uint iB = spring.ParticleIndiceB;

    Particle A = particles[iA];
    Particle B = particles[iB];

    float3 posDifference = B.Pos - A.Pos;
    float len = length(posDifference);
    if (len < 0.00001)
    {
        return;
    }

    float3 dir = posDifference / len;
    float diff = len - spring.restLength;

    float invA = A.IsPinned ? 0 : 1 / A.mass;
    float invB = B.IsPinned ? 0 : 1 / B.mass;
    float sum = invA + invB;

    if (sum > 0)
    {
        float3 corr = dir * diff;

        int3 packedA = int3(corr * (invA / sum) * FIXED_SCALE);
        int3 packedB = int3(-corr * (invB / sum) * FIXED_SCALE);

        if (!A.IsPinned)
        {
            InterlockedAdd(positionDeltas[iA].x, packedA.x);
            InterlockedAdd(positionDeltas[iA].y, packedA.y);
            InterlockedAdd(positionDeltas[iA].z, packedA.z);
            InterlockedAdd(deltaCounts[iA], 1);
        }

        if (!B.IsPinned)
        {
            InterlockedAdd(positionDeltas[iB].x, packedB.x);
            InterlockedAdd(positionDeltas[iB].y, packedB.y);
            InterlockedAdd(positionDeltas[iB].z, packedB.z);
            InterlockedAdd(deltaCounts[iB], 1);
        }
        
    }

    particles[iA] = A;
    particles[iB] = B;
}

//apply the corrections aftyer to avoid race condition
[numthreads(1024, 1, 1)]
void CS_ApplyCorrections(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = particles[i];

    if (!p.IsPinned && deltaCounts[i] > 0)
    {
        float3 corr = float3(positionDeltas[i]) / FIXED_SCALE / deltaCounts[i];

        p.Pos += corr;
    }

    positionDeltas[i] = float3(0, 0, 0);
    deltaCounts[i] = 0;

    particles[i] = p;
}

//simulate velocity changes on 256 threads
[numthreads(1024, 1, 1)]
void CS_UpdateVelocity(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;

    Particle p = particles[i];

    if (!p.IsPinned)
    {
        p.Velocity = (p.Pos - p.PrevPos) / dt;
    }

    particles[i] = p;
}
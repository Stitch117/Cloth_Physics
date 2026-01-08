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

cbuffer ClothSimParams : register(b0)
{
    float dt;
    float3 gravity;
    int constraintIterations;
    int numOfSprings;
    int maxStretchLimit;
    int PAD;
};

[numthreads(256, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint i = DTid.x;

    Particle p = particles[i];
    
    if (p.IsPinned)
    {
        particles[i] = p;
        return;
    }
    
    // Apply gravity
    p.Velocity += gravity * dt;
    p.PrevPos = p.Pos;
    p.Pos += p.Velocity * dt;
    
    // Apply spring forces as positional constraints
    for (int iter = 0; iter < constraintIterations; iter++)
    {
        for (uint s = 0; s < numOfSprings; s++)
        {
            Spring spring = clothSprings[s];
            
            int otherIndex;
            float sign;
            
            if (spring.ParticleIndiceA == i)
            {
                otherIndex = spring.ParticleIndiceB;
                sign = 1.0f;
            }
            else if (spring.ParticleIndiceB == i)
            {
                otherIndex = spring.ParticleIndiceA;
                sign = -1.0f;
            }
            else
            {
                continue;
            }
            
            Particle other = particles[otherIndex];
            
            float3 PosDif = other.Pos - p.Pos;
            float currentLength = length(PosDif);
            if (currentLength < 0.00001f)
            {
                continue;
            }
            
            float maxLength = maxStretchLimit * spring.restLength;
            
            if (currentLength > maxLength)
            {
                float3 dir = PosDif / currentLength;
                float overStretch = currentLength - maxLength;

                float invMassP = p.IsPinned ? 0.0f : 1.0f / p.mass;
                float invMassO = other.IsPinned ? 0.0f : 1.0f / other.mass;
                float totalInvMass = invMassP + invMassO;

                if (totalInvMass > 0.0f)
                {
                    float correction = overStretch * (invMassP / totalInvMass);
                    p.Velocity += sign * dir * correction;
                }
            }
        }
    }
    
    
    // Update velocity based on positional change
    p.Velocity = (p.Pos - p.PrevPos) / dt;
    p.Pos += p.Velocity * dt;

    particles[i] = p;
}

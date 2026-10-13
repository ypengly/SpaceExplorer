#include "SEEnvironment.h"
#include "SETerrainStreamer.h"
#include "SEProgression.h"
#include "SEVisual.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/SkyAtmosphere.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

ASEEnvironment::ASEEnvironment()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ASEEnvironment::Init(const FSEPlanetData& P, const FLinearColor& StarColor, ASETerrainStreamer* T)
{
    PlanetData = P; Terrain = T;
    Progression = GetGameInstance()->GetSubsystem<USEProgression>();
    UWorld* W = GetWorld();

    // sun (flagged as the atmosphere sun before it registers)
    Sun = W->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform::Identity);
    if (UDirectionalLightComponent* LC = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
    {
        LC->SetMobility(EComponentMobility::Movable);
        LC->bUsedAsAtmosphereSunLight = P.bHasAtmosphere;
    }
    Sun->FinishSpawning(FTransform::Identity);
    Sun->GetLightComponent()->SetLightColor(StarColor);

    Sky = W->SpawnActor<ASkyLight>();
    if (Sky)
    {
        Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sky->GetLightComponent()->bRealTimeCapture = true;
        Sky->GetLightComponent()->SetIntensity(P.bHasAtmosphere ? 1.f : 0.15f);
    }

    if (P.bHasAtmosphere)
    {
        Atmosphere = W->SpawnActor<ASkyAtmosphere>();
        if (Atmosphere)
        {
            if (USkyAtmosphereComponent* AC = Atmosphere->FindComponentByClass<USkyAtmosphereComponent>())
            {
                if (P.Biome == ESEBiome::Volcanic) AC->SetRayleighScattering(FLinearColor(0.12f, 0.03f, 0.02f));
                else if (P.Biome == ESEBiome::Desert) AC->SetRayleighScattering(FLinearColor(0.1f, 0.06f, 0.03f));
            }
        }
        Fog = W->SpawnActor<AExponentialHeightFog>();
        if (Fog) Fog->GetComponent()->SetFogDensity(0.0008f);
    }

    // ocean surface
    const float Sea = T ? T->GetSeaLevel() : -1.0e9f;
    Water = NewObject<UStaticMeshComponent>(this);
    Water->SetMobility(EComponentMobility::Movable);
    Water->SetupAttachment(RootComponent);
    Water->RegisterComponent();
    SEVisual::Setup(Water, TEXT("Plane"), FLinearColor(0.04f, 0.22f, 0.36f), FVector(2500.f, 2500.f, 1.f));
    Water->SetCollisionProfileName(TEXT("NoCollision"));
    Water->SetCastShadow(false);
    Water->SetVisibility(Sea > -1.0e8f);
    if (Sea > -1.0e8f) Water->SetWorldLocation(FVector(0.f, 0.f, Sea));

    // weather particles (instanced meshes recycled around the player)
    switch (P.Biome)
    {
    case ESEBiome::Temperate: case ESEBiome::Ocean: PrecipName = TEXT("Rain"); bHasPrecip = true; FallSpeed = 2200.f; break;
    case ESEBiome::Frozen:   PrecipName = TEXT("Snow"); bHasPrecip = true; FallSpeed = 350.f; break;
    case ESEBiome::Desert:   PrecipName = TEXT("Dust storm"); bHasPrecip = true; FallSpeed = 1800.f; break;
    case ESEBiome::Volcanic: PrecipName = TEXT("Ash fall"); bHasPrecip = true; FallSpeed = 300.f; break;
    default: bHasPrecip = false; break;
    }
    bHasPrecip = bHasPrecip && P.bHasAtmosphere;
    if (bHasPrecip)
    {
        Particles = NewObject<UInstancedStaticMeshComponent>(this);
        Particles->SetMobility(EComponentMobility::Movable);
        Particles->SetupAttachment(RootComponent);
        Particles->RegisterComponent();
        Particles->SetStaticMesh(SEVisual::Basic(TEXT("Cube")));
        const FLinearColor Col = PrecipName == TEXT("Rain") ? FLinearColor(0.6f, 0.7f, 0.9f) : (PrecipName == TEXT("Snow") ? FLinearColor::White : (PrecipName == TEXT("Dust storm") ? FLinearColor(0.75f, 0.55f, 0.3f) : FLinearColor(0.15f, 0.13f, 0.13f)));
        SEVisual::Colorize(Particles, Col);
        Particles->SetCollisionProfileName(TEXT("NoCollision"));
        Particles->SetCastShadow(false);
        Particles->SetVisibility(false);
        FRandomStream R(P.Seed);
        ParticleScale = PrecipName == TEXT("Rain") ? FVector(0.03f, 0.03f, 0.9f) : (PrecipName == TEXT("Dust storm") ? FVector(0.5f, 0.1f, 0.1f) : FVector(0.08f));
        for (int32 i = 0; i < 220; ++i)
        {
            Offsets.Add(FVector(R.FRandRange(-2500.f, 2500.f), R.FRandRange(-2500.f, 2500.f), R.FRandRange(0.f, 2500.f)));
            Particles->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, ParticleScale));
        }
    }
    WeatherTimer = FMath::FRandRange(60.f, 150.f);
    bInit = true;
}

bool ASEEnvironment::IsNight() const
{
    const float H = Progression ? Progression->Data.WorldHours : 12.f;
    return H < 6.f || H >= 20.f;
}

FString ASEEnvironment::GetWeatherName() const
{
    if (!bHasPrecip) return PlanetData.bHasAtmosphere ? TEXT("Clear") : TEXT("Vacuum");
    return Intensity > 0.3f ? PrecipName : TEXT("Clear");
}

void ASEEnvironment::UpdateWeather(float Dt, const FVector& PlayerLoc)
{
    if (!bHasPrecip) return;
    WeatherTimer -= Dt;
    if (WeatherTimer <= 0.f)
    {
        TargetIntensity = (TargetIntensity > 0.5f) ? 0.f : FMath::FRandRange(0.6f, 1.f);
        WeatherTimer = FMath::FRandRange(90.f, 240.f);
    }
    Intensity = FMath::FInterpConstantTo(Intensity, TargetIntensity, Dt, 0.12f);
    const bool bShow = Intensity > 0.05f;
    Particles->SetVisibility(bShow);
    if (!bShow) return;

    TArray<FTransform> Out; Out.Reserve(Offsets.Num());
    const bool bDust = PrecipName == TEXT("Dust storm");
    for (int32 i = 0; i < Offsets.Num(); ++i)
    {
        const FVector& O = Offsets[i];
        FVector L;
        if (bDust)
        {
            L = PlayerLoc + FVector(FMath::Fmod(O.X + 2500.f + Clock * FallSpeed, 5000.f) - 2500.f, O.Y, O.Z * 0.5f - 300.f);
        }
        else
        {
            const float Z = FMath::Fmod(O.Z - Clock * FallSpeed + 100000.f, 2500.f);
            L = PlayerLoc + FVector(O.X, O.Y, Z - 400.f);
        }
        Out.Add(FTransform(FRotator::ZeroRotator, L, ParticleScale * (i < Offsets.Num() * Intensity ? 1.f : 0.f)));
    }
    Particles->BatchUpdateInstancesTransforms(0, Out, true, true, true);
    if (Fog) Fog->GetComponent()->SetFogDensity(0.0008f + 0.012f * Intensity * (PrecipName == TEXT("Dust storm") ? 1.5f : 1.f));
}

void ASEEnvironment::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!bInit || !Progression) return;
    Clock += Dt;

    // advance world time
    Progression->Data.WorldHours = FMath::Fmod(Progression->Data.WorldHours + Dt * 24.f / (DayLengthMinutes * 60.f), 24.f);
    const float Hours = Progression->Data.WorldHours;

    if (Sun)
    {
        Sun->SetActorRotation(FRotator(-(Hours - 6.f) * 15.f, 40.f, 0.f));
        const float Day = FMath::Max(0.f, -FMath::Sin(FMath::DegreesToRadians(-(Hours - 6.f) * 15.f) * -1.f * -1.f));
        const float Lux = PlanetData.bHasAtmosphere ? 7.f : 9.f;
        Sun->GetLightComponent()->SetIntensity(Lux * Day * (1.f - 0.45f * Intensity) + 0.03f);
    }

    APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
    const FVector Loc = P ? P->GetActorLocation() : FVector::ZeroVector;
    if (Water && Water->IsVisible()) { Water->SetWorldLocation(FVector(Loc.X, Loc.Y, Terrain->GetSeaLevel())); }
    UpdateWeather(Dt, Loc);
}

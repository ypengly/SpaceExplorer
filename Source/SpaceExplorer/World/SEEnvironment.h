#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SEGalaxyTypes.h"
#include "SEEnvironment.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class ASkyAtmosphere;
class ASETerrainStreamer;
class USEProgression;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;

/** Sun and day/night cycle, sky atmosphere, fog, ocean surface and weather for the current planet. */
UCLASS()
class SPACEEXPLORER_API ASEEnvironment : public AActor
{
    GENERATED_BODY()
public:
    ASEEnvironment();
    void Init(const FSEPlanetData& Planet, const FLinearColor& StarColor, ASETerrainStreamer* Terrain);
    virtual void Tick(float Dt) override;

    UPROPERTY(EditAnywhere, Category="Environment") float DayLengthMinutes = 20.f;
    FString GetWeatherName() const;
    bool IsNight() const;

private:
    void UpdateWeather(float Dt, const FVector& PlayerLoc);

    UPROPERTY() TObjectPtr<ADirectionalLight> Sun;
    UPROPERTY() TObjectPtr<ASkyLight> Sky;
    UPROPERTY() TObjectPtr<AExponentialHeightFog> Fog;
    UPROPERTY() TObjectPtr<ASkyAtmosphere> Atmosphere;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Water;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Particles;
    UPROPERTY() TObjectPtr<ASETerrainStreamer> Terrain;
    UPROPERTY() TObjectPtr<USEProgression> Progression;

    FSEPlanetData PlanetData;
    TArray<FVector> Offsets;
    float WeatherTimer = 120.f, Intensity = 0.f, TargetIntensity = 0.f, Clock = 0.f;
    float FallSpeed = 1500.f;
    bool bHasPrecip = false, bInit = false;
    FString PrecipName;
    FVector ParticleScale = FVector(0.1f);
};

#pragma once
#include "CoreMinimal.h"
#include "SEGalaxyTypes.generated.h"

UENUM(BlueprintType)
enum class ESEBiome : uint8 { Temperate, Desert, Frozen, Volcanic, Ocean, Barren };

USTRUCT(BlueprintType)
struct FSEPlanetData
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
    UPROPERTY(BlueprintReadOnly) ESEBiome Biome = ESEBiome::Barren;
    UPROPERTY(BlueprintReadOnly) float RadiusKm = 6000.f;
    /** Multiple of Earth gravity. */
    UPROPERTY(BlueprintReadOnly) float Gravity = 1.f;
    UPROPERTY(BlueprintReadOnly) float OrbitAU = 1.f;
    UPROPERTY(BlueprintReadOnly) float TemperatureC = 15.f;
    UPROPERTY(BlueprintReadOnly) bool bHasAtmosphere = true;
    UPROPERTY(BlueprintReadOnly) bool bBreathable = false;
    UPROPERTY(BlueprintReadOnly) bool bIsMoon = false;
};

USTRUCT(BlueprintType)
struct FSEStarSystemData
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Index = 0;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
    /** Galactic position in light years. */
    UPROPERTY(BlueprintReadOnly) FVector PositionLY = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FString StarClass;
    UPROPERTY(BlueprintReadOnly) FLinearColor StarColor = FLinearColor::White;
    UPROPERTY(BlueprintReadOnly) int32 AsteroidFieldCount = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FSEPlanetData> Planets;
};

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SEVitalsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVitalsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDied);

/** Health, oxygen and energy. Drives the HUD and is serialized by the save system. */
UCLASS(ClassGroup=(SpaceExplorer), meta=(BlueprintSpawnableComponent))
class SPACEEXPLORER_API USEVitalsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USEVitalsComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals") float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals") float MaxOxygen = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals") float MaxEnergy = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals|Rates") float OxygenDrainPerSec = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals|Rates") float OxygenRefillPerSec = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals|Rates") float SuffocationDamagePerSec = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals|Rates") float EnergyRegenPerSec = 8.f;

    /** Set false on planets without breathable air; oxygen then drains. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals") bool bInBreathableAir = true;
    /** True inside a habitat or oxygen beacon radius. */
    UPROPERTY(BlueprintReadWrite, Category="Vitals") bool bSheltered = false;
    /** True while the head is under water. */
    UPROPERTY(BlueprintReadWrite, Category="Vitals") bool bUnderwater = false;

    UPROPERTY(BlueprintReadOnly, Category="Vitals") float Health;
    UPROPERTY(BlueprintReadOnly, Category="Vitals") float Oxygen;
    UPROPERTY(BlueprintReadOnly, Category="Vitals") float Energy;

    UPROPERTY(BlueprintAssignable) FOnVitalsChanged OnVitalsChanged;
    UPROPERTY(BlueprintAssignable) FOnPlayerDied OnPlayerDied;

    UFUNCTION(BlueprintCallable, Category="Vitals") void ApplyDamage(float Amount);
    UFUNCTION(BlueprintCallable, Category="Vitals") void Heal(float Amount);
    void RestoreEnergy(float Amount) { Energy = FMath::Min(MaxEnergy, Energy + Amount); }
    /** Returns false (and consumes nothing) if not enough energy. */
    UFUNCTION(BlueprintCallable, Category="Vitals") bool ConsumeEnergy(float Amount);
    UFUNCTION(BlueprintCallable, Category="Vitals") void SetVitals(float InHealth, float InOxygen, float InEnergy);
    UFUNCTION(BlueprintPure, Category="Vitals") bool IsDead() const { return Health <= 0.f; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    float RegenCooldown = 0.f;
};

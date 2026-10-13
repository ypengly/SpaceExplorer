#include "SEVitalsComponent.h"

USEVitalsComponent::USEVitalsComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    Health = MaxHealth; Oxygen = MaxOxygen; Energy = MaxEnergy;
}

void USEVitalsComponent::BeginPlay()
{
    Super::BeginPlay();
    Health = MaxHealth; Oxygen = MaxOxygen; Energy = MaxEnergy;
    OnVitalsChanged.Broadcast();
}

void USEVitalsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (IsDead()) return;

    const bool bAir = (bInBreathableAir || bSheltered) && !bUnderwater;
    if (bAir)
    {
        Oxygen = FMath::Min(MaxOxygen, Oxygen + OxygenRefillPerSec * DeltaTime);
    }
    else
    {
        Oxygen = FMath::Max(0.f, Oxygen - (bUnderwater ? 3.f : 1.f) * OxygenDrainPerSec * DeltaTime);
        if (Oxygen <= 0.f) { ApplyDamage(SuffocationDamagePerSec * DeltaTime); }
    }

    RegenCooldown = FMath::Max(0.f, RegenCooldown - DeltaTime);
    if (RegenCooldown <= 0.f)
    {
        Energy = FMath::Min(MaxEnergy, Energy + EnergyRegenPerSec * DeltaTime);
    }
    OnVitalsChanged.Broadcast();
}

void USEVitalsComponent::ApplyDamage(float Amount)
{
    if (IsDead() || Amount <= 0.f) return;
    Health = FMath::Max(0.f, Health - Amount);
    OnVitalsChanged.Broadcast();
    if (Health <= 0.f) { OnPlayerDied.Broadcast(); }
}

void USEVitalsComponent::Heal(float Amount)
{
    if (IsDead() || Amount <= 0.f) return;
    Health = FMath::Min(MaxHealth, Health + Amount);
    OnVitalsChanged.Broadcast();
}

bool USEVitalsComponent::ConsumeEnergy(float Amount)
{
    if (Energy < Amount) return false;
    Energy -= Amount;
    RegenCooldown = 1.0f;
    return true;
}

void USEVitalsComponent::SetVitals(float InHealth, float InOxygen, float InEnergy)
{
    Health = FMath::Clamp(InHealth, 0.f, MaxHealth);
    Oxygen = FMath::Clamp(InOxygen, 0.f, MaxOxygen);
    Energy = FMath::Clamp(InEnergy, 0.f, MaxEnergy);
    OnVitalsChanged.Broadcast();
}

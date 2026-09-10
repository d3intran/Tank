#include "TankHealth.h"
#include "Tank.h"
#include "Net/UnrealNetwork.h"

UTankHealth::UTankHealth()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTankHealth::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTankHealth, CurrentHealth);
}

void UTankHealth::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
}

void UTankHealth::OnRep_CurrentHealth()
{
	UE_LOG(LogTank, Log, TEXT("[Battle] %s 血量同步至 %.0f/%.0f"),
		*GetNameSafe(GetOwner()), CurrentHealth, MaxHealth);
}

float UTankHealth::ApplyDamage(float DamageAmount)
{
	if (bDepleted || DamageAmount <= 0.0f)
	{
		return CurrentHealth;
	}
	CurrentHealth = FMath::Max(CurrentHealth - DamageAmount, 0.0f);
	OnHealthChanged.Broadcast(CurrentHealth);
	if (CurrentHealth <= 0.0f)
	{
		bDepleted = true;
		OnDepleted.Broadcast();
		UE_LOG(LogTank, Log, TEXT("[Battle] %s 血量耗尽"), *GetNameSafe(GetOwner()));
	}
	return CurrentHealth;
}

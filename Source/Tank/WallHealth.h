#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WallHealth.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWallHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWallDepleted);

/**
 * 城墙血量组件（P1 灰盒）：纯逻辑组件，可挂到任意宿主 Actor（灰盒方块墙或后续 CityGate）。
 * 丧尸攻击态持续扣血，归零广播 OnWallDepleted → GameMode 判负。
 */
UCLASS(ClassGroup = (Defense), meta = (BlueprintSpawnableComponent))
class TANK_API UWallHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWallHealthComponent();

	// 扣血入口（丧尸啃咬/未来其他伤害源）。返回扣后剩余值，已耗尽时幂等返回 0
	float ApplyDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Wall|Health")
	FORCEINLINE float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Wall|Health")
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Wall|Health")
	FORCEINLINE bool IsDepleted() const { return bDepleted; }

	// 事件
	UPROPERTY(BlueprintAssignable, Category = "Wall|Events")
	FOnWallHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Wall|Events")
	FOnWallDepleted OnDepleted;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall|Health", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1000.0"))
	float MaxHealth = 5000.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall|Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 5000.0f;

	bool bDepleted = false;
};

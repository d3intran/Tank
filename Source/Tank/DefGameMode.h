#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DefGameMode.generated.h"

class ATankPawn;
class ADefHUD;
class ADefWall;
class AZombieSpawner;
class UWallHealthComponent;

UENUM(BlueprintType)
enum class EDefMatchState : uint8
{
	InProgress UMETA(DisplayName = "防守中"),
	Won        UMETA(DisplayName = "守住了"),
	Lost       UMETA(DisplayName = "城墙陷落")
};

/**
 * P1 灰盒胜负判定：城墙血量归零=输；尸潮刷完且场上无活丧尸=赢。
 * 任一结局 5s 后自动重开本关（灰盒快速迭代节奏，正式菜单/波次在 P4）。
 * 单机 standalone，无复制逻辑。
 */
UCLASS(Blueprintable)
class TANK_API ADefGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADefGameMode();

	UFUNCTION(BlueprintPure, Category = "Defense|State")
	FORCEINLINE EDefMatchState GetMatchState() const { return MatchState; }

	UFUNCTION(BlueprintPure, Category = "Defense|State")
	FORCEINLINE float GetRestartCountdown() const { return RestartCountdown; }

	// HUD 每帧读取（GameMode 侧 0.5s 节流缓存，避免 HUD 全帧迭代 Actor）
	UFUNCTION(BlueprintPure, Category = "Defense|State")
	UWallHealthComponent* GetWallHealth() const { return CachedWallHealth.Get(); }

	UFUNCTION(BlueprintPure, Category = "Defense|State")
	FORCEINLINE int32 GetAliveZombieCount() const { return LastAliveZombieCount; }

protected:
	virtual void BeginPlay() override;

private:
	void CheckMatch();
	void EndMatch(EDefMatchState Outcome);
	int32 CountAliveZombies() const;

	EDefMatchState MatchState = EDefMatchState::InProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense|Config", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "3.0", Units = "s"))
	float RestartDelay = 5.0f;

	float RestartCountdown = 0.0f;
	int32 LastAliveZombieCount = 0;

	TWeakObjectPtr<UWallHealthComponent> CachedWallHealth;
	TWeakObjectPtr<AZombieSpawner> CachedSpawner;

	FTimerHandle MatchCheckHandle;
};

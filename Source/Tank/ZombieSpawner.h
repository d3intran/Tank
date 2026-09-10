#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZombieSpawner.generated.h"

class AZombie;
class USceneComponent;

/**
 * P1 灰盒刷怪器：从自身位置向目标墙刷新丧尸，间隔刷完全部配额。
 * P2 迁 Mass 时此类替换为 MassSpawner 配置，行为链（刷新→冲墙）语义不变。
 */
UCLASS(Blueprintable)
class TANK_API AZombieSpawner : public AActor
{
	GENERATED_BODY()

public:
	AZombieSpawner();

	// 配额是否刷完
	bool IsFinished() const { return RemainingToSpawn <= 0; }

protected:
	virtual void BeginPlay() override;

private:
	void StartWave();
	void SpawnOne();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawner|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawner|Config", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AZombie> ZombieClass;

	// 目标墙：丧尸的寻的终点（BeginPlay 前注入每只丧尸）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Config", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> TargetWall;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Config", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "20", ClampMax = "800"))
	int32 TotalToSpawn = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Config", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.5", Units = "s"))
	float SpawnInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Config", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "1.0", Units = "s"))
	float InitialDelay = 2.0f;

	// 出生点抖动半边长（XY），避免全部丧尸叠在一点
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Config", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float SpawnJitterExtent = 200.0f;

	// BeginPlay 自动开始刷怪；关掉则手动调 StartWave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Config", meta = (AllowPrivateAccess = "true"))
	bool bAutoStart = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Spawner|State", meta = (AllowPrivateAccess = "true"))
	int32 RemainingToSpawn = 0;

	FTimerHandle SpawnTimerHandle;
};

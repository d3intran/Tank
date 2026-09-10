#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DefWall.generated.h"

class UStaticMeshComponent;
class UWallHealthComponent;
class UClimbManager;

/**
 * P1/P2a 灰盒城墙：方块体 + 血量组件 + 聚集闸门。尺寸对齐真实城墙 7.74m 高。
 * P2 换 CityGate 宿主时把 WallHealth + ClimbManager 挂过去，丧尸/GameMode 逻辑零改动。
 */
UCLASS(Blueprintable)
class TANK_API ADefWall : public AActor
{
	GENERATED_BODY()

public:
	ADefWall();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall|Components")
	TObjectPtr<UStaticMeshComponent> WallMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall|Components")
	TObjectPtr<UWallHealthComponent> WallHealth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall|Components")
	TObjectPtr<UClimbManager> ClimbManager;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClimbManager.generated.h"

class AZombie;

/**
 * P2a 尸堆槽位系统：挂在隐形代理墙上，预计算三维槽位网格。
 * 丧尸进堆区领一个空闲槽位 → lerp 过去占住；所有槽满后新来的沿墙垂直上升登顶扣血。
 * 槽位排列：底层最宽（多排多列），逐层收窄形成锥形堆体视觉。
 */
UCLASS(ClassGroup = (Defense), meta = (BlueprintSpawnableComponent))
class TANK_API UClimbManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UClimbManager();

	void EnsureOrientationFrom(const FVector& FromLoc);

	FVector GetPileCenter() const { return PileCenter; }
	FVector GetOutwardDir() const { return OutwardDir; }
	float GetPileRadius() const { return PileRadius; }

	int32 AcquireSlot();
	void ReleaseSlot(int32 SlotIndex);
	FVector GetSlotLocation(int32 SlotIndex) const;
	bool IsSlotOccupied(int32 SlotIndex) const;
	int32 GetTotalSlots() const { return Slots.Num(); }

	bool IsPileFull() const;

	void NotifyTopOut(AZombie* Zombie);
	float GetWallTopZ() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Pile", meta = (AllowPrivateAccess = "true", ClampMin = "200.0", Units = "cm"))
	float PileRadius = 774.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Pile", meta = (AllowPrivateAccess = "true", ClampMin = "60.0", UIMin = "80.0", Units = "cm"))
	float SlotSpacingHorizontal = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Pile", meta = (AllowPrivateAccess = "true", ClampMin = "40.0", UIMin = "60.0", Units = "cm"))
	float SlotSpacingVertical = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Pile", meta = (AllowPrivateAccess = "true", ClampMin = "60.0", UIMin = "80.0", Units = "cm"))
	float SlotSpacingDepth = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Gate", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "50.0"))
	float TopOutDamage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Pile", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "3"))
	int32 MaxLayers = 8;

private:
	void BuildSlotGrid();

	struct FSlotData
	{
		FVector Location;
		bool bOccupied = false;
	};

	TArray<FSlotData> Slots;

	bool bOrientationCached = false;
	bool bSlotsBuilt = false;
	FVector OutwardDir = FVector::XAxisVector;
	FVector WallTangent = FVector::YAxisVector;
	FVector PileCenter = FVector::ZeroVector;
};

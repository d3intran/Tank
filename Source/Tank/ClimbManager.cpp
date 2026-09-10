#include "ClimbManager.h"
#include "Tank.h"
#include "Zombie.h"
#include "WallHealth.h"

UClimbManager::UClimbManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UClimbManager::EnsureOrientationFrom(const FVector& FromLoc)
{
	if (bOrientationCached)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	const FBox Box = Owner->GetComponentsBoundingBox();
	const FVector Center = Box.GetCenter();
	const FVector Extent = Box.GetExtent();

	if (Extent.GetMax() < 1.0f)
	{
		UE_LOG(LogTank, Warning, TEXT("[Climb] 宿主包围盒为零，跳过标定（下一只丧尸重试）"));
		return;
	}

	FVector::FReal MinExtent = Extent.X;
	int32 ThinAxis = 0;
	if (Extent.Y < MinExtent) { MinExtent = Extent.Y; ThinAxis = 1; }
	if (Extent.Z < MinExtent) { MinExtent = Extent.Z; ThinAxis = 2; }

	FVector Axis = FVector::XAxisVector;
	if (ThinAxis == 1) Axis = FVector::YAxisVector;
	else if (ThinAxis == 2) Axis = FVector::ZAxisVector;

	OutwardDir = Axis.GetSafeNormal();
	if (FVector::DotProduct(FromLoc - Center, OutwardDir) < 0.0f)
	{
		OutwardDir = -OutwardDir;
	}

	WallTangent = FVector::CrossProduct(FVector::UpVector, OutwardDir).GetSafeNormal();
	PileCenter = Center + OutwardDir * (float)Extent[ThinAxis];
	PileCenter.Z = 0.0f;
	bOrientationCached = true;

	BuildSlotGrid();

	UE_LOG(LogTank, Log, TEXT("[Climb] 槽位标定：峰心=(%.0f,%.0f) Outward=%s Tangent=%s 总槽位=%d"),
		PileCenter.X, PileCenter.Y, *OutwardDir.ToString(), *WallTangent.ToString(), Slots.Num());
}

void UClimbManager::BuildSlotGrid()
{
	Slots.Empty();

	const float WallTop = GetWallTopZ();
	UE_LOG(LogTank, Log, TEXT("[Climb] BuildSlotGrid: WallTop=%.0f PileRadius=%.0f MaxLayers=%d"),
		WallTop, PileRadius, MaxLayers);
	if (WallTop <= 0.0f)
	{
		UE_LOG(LogTank, Warning, TEXT("[Climb] WallTop<=0，无法生成槽位！"));
		return;
	}

	for (int32 Layer = 0; Layer < MaxLayers; ++Layer)
	{
		const float LayerZ = Layer * SlotSpacingVertical;
		if (LayerZ >= WallTop)
		{
			break;
		}

		const float LayerRadius = PileRadius * (1.0f - (float)Layer / MaxLayers);
		if (LayerRadius < SlotSpacingDepth * 0.5f)
		{
			continue;
		}

		const int32 DepthCount = FMath::Max(1, FMath::FloorToInt(LayerRadius / SlotSpacingDepth));
		const float HalfWidth = LayerRadius * 0.8f;
		const int32 ColCount = FMath::Max(1, FMath::FloorToInt(HalfWidth * 2.0f / SlotSpacingHorizontal));

		const int32 LayerStart = Slots.Num();
		for (int32 d = 0; d < DepthCount; ++d)
		{
			const float DepthOffset = (d + 1) * SlotSpacingDepth;
			for (int32 c = 0; c < ColCount; ++c)
			{
				const float LateralOffset = ((float)c - (ColCount - 1) * 0.5f) * SlotSpacingHorizontal;

				FVector Loc = PileCenter
					+ OutwardDir * DepthOffset
					+ WallTangent * LateralOffset;
				Loc.Z = LayerZ;

				Slots.Add({ Loc, false });
			}
		}
		UE_LOG(LogTank, Log, TEXT("[Climb] Layer %d: Z=%.0f Radius=%.0f Depth=%d Cols=%d → %d slots"),
			Layer, LayerZ, LayerRadius, DepthCount, ColCount, Slots.Num() - LayerStart);
	}

	bSlotsBuilt = true;
	UE_LOG(LogTank, Log, TEXT("[Climb] 槽位网格生成完毕：共 %d 个槽位"), Slots.Num());
}

int32 UClimbManager::AcquireSlot()
{
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (!Slots[i].bOccupied)
		{
			Slots[i].bOccupied = true;
			return i;
		}
	}
	return INDEX_NONE;
}

void UClimbManager::ReleaseSlot(int32 SlotIndex)
{
	if (Slots.IsValidIndex(SlotIndex))
	{
		Slots[SlotIndex].bOccupied = false;
	}
}

FVector UClimbManager::GetSlotLocation(int32 SlotIndex) const
{
	if (Slots.IsValidIndex(SlotIndex))
	{
		return Slots[SlotIndex].Location;
	}
	return PileCenter;
}

bool UClimbManager::IsSlotOccupied(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex].bOccupied;
}

bool UClimbManager::IsPileFull() const
{
	for (const FSlotData& Slot : Slots)
	{
		if (!Slot.bOccupied)
		{
			return false;
		}
	}
	return Slots.Num() > 0;
}

void UClimbManager::NotifyTopOut(AZombie* Zombie)
{
	UWallHealthComponent* Wall = GetOwner() ? GetOwner()->FindComponentByClass<UWallHealthComponent>() : nullptr;
	if (Wall)
	{
		Wall->ApplyDamage(TopOutDamage);
	}
	UE_LOG(LogTank, Log, TEXT("[Climb] %s 登顶，墙扣 %.0f"), Zombie ? *Zombie->GetName() : TEXT("?"), TopOutDamage);
}

float UClimbManager::GetWallTopZ() const
{
	if (AActor* Owner = GetOwner())
	{
		return Owner->GetComponentsBoundingBox().Max.Z;
	}
	return 0.0f;
}

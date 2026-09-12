#include "TankVehicle.h"
#include "TankVehicleLayout.h"

#include "ChaosVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Tank.h"

using namespace TankVehicleLayout;

// ============================================================================
// 底盘物理盒校正（双阶梯防托底碰撞盒）
// ============================================================================
void ATankVehicle::EnsureChassisPhysicsBox(USkeletalMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}

	UPhysicsAsset* PhysAsset = Mesh->GetPhysicsAsset();
	if (!PhysAsset)
	{
		return;
	}

	bool bModified = false;
	for (TObjectPtr<USkeletalBodySetup>& BodySetup : PhysAsset->SkeletalBodySetups)
	{
		if (BodySetup && BodySetup->BoneName == FName(TEXT("root")))
		{
			bool bNeedsReplace = false;
			if (BodySetup->AggGeom.BoxElems.Num() != 2)
			{
				bNeedsReplace = true;
			}
			else
			{
				const FKBoxElem& MainBox = BodySetup->AggGeom.BoxElems[0];
				const FKBoxElem& NoseBox = BodySetup->AggGeom.BoxElems[1];
				if (!FMath::IsNearlyEqual(MainBox.X, 560.0f, 1.0f) ||
					!FMath::IsNearlyEqual(MainBox.Y, 340.0f, 1.0f) ||
					!FMath::IsNearlyEqual(MainBox.Z, 120.0f, 1.0f) ||
					!FMath::IsNearlyEqual(MainBox.Center.X, -60.0f, 1.0f) ||
					!FMath::IsNearlyEqual(MainBox.Center.Z, 95.0f, 1.0f) ||
					!FMath::IsNearlyEqual(NoseBox.X, 100.0f, 1.0f) ||
					!FMath::IsNearlyEqual(NoseBox.Y, 320.0f, 1.0f) ||
					!FMath::IsNearlyEqual(NoseBox.Z, 75.0f, 1.0f) ||
					!FMath::IsNearlyEqual(NoseBox.Center.X, 270.0f, 1.0f) ||
					!FMath::IsNearlyEqual(NoseBox.Center.Z, 107.5f, 1.0f))
				{
					bNeedsReplace = true;
				}
			}

			if (bNeedsReplace)
			{
				BodySetup->RemoveSimpleCollision();

				// 盒 1：主车体装甲盒（中后部与动力舱），X从-340到+220，高从Z=35到155
				// 盒底相对履带留空 +35cm（世界 17.5cm），悬挂与凸起地形绝不托底
				FKBoxElem MainBox;
				MainBox.Center = FVector(-60.0f, 0.0f, 95.0f);
				MainBox.X = 560.0f;
				MainBox.Y = 340.0f;
				MainBox.Z = 120.0f;
				BodySetup->AggGeom.BoxElems.Add(MainBox);

				// 盒 2：车头首上/首下接近角保护盒，X从+220到+320，高从Z=70到145
				// 盒底抬高至 +70cm（世界 35cm），留出 >38° 接近角，彻底杜绝 30° 坡道卡死
				FKBoxElem NoseBox;
				NoseBox.Center = FVector(270.0f, 0.0f, 107.5f);
				NoseBox.X = 100.0f;
				NoseBox.Y = 320.0f;
				NoseBox.Z = 75.0f;
				BodySetup->AggGeom.BoxElems.Add(NoseBox);

				BodySetup->InvalidatePhysicsData();
				BodySetup->CreatePhysicsMeshes();

				PhysAsset->UpdateBoundsBodiesArray();
				PhysAsset->UpdateBodySetupIndexMap();
				PhysAsset->MarkPackageDirty();
				bModified = true;

				UE_LOG(LogTank, Log, TEXT("[Vehicle] 自动校正物理资产 %s root 刚体为双阶梯防托底碰撞盒：主盒(560×340×120 心=(-60,0,95) 底+35cm) + 车头盒(100×320×75 心=(270,0,107.5) 底+70cm接近角保护)"),
					*PhysAsset->GetName());
			}
			break;
		}
	}

	if (bModified && Mesh->IsPhysicsStateCreated())
	{
		Mesh->RecreatePhysicsState();
	}
}

// ============================================================================
// 出生落位（下探 12 轮射线贴地，杜绝半空悬浮）
// ============================================================================
void ATankVehicle::SnapToGroundOnSpawn()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Scale = FMath::Max(VehicleScale, 0.01f);
	const FRotator YawOnly(0.0f, GetActorRotation().Yaw, 0.0f);
	const FVector Up(0.0f, 0.0f, SpawnSnapProbeUp);
	const FVector Down(0.0f, 0.0f, SpawnSnapProbeDown);

	float GroundZ = -FLT_MAX;
	int32 HitCount = 0;
	int32 MissedCount = 0;
	for (int32 i = 0; i < RoadWheels.Num(); ++i)
	{
		const bool bRight = RoadSetups[i].bRightSide;
		const FVector WheelOffset = YawOnly.RotateVector(FVector(
			RoadSetups[i].X * Scale, (bRight ? RoadWheelY : -RoadWheelY) * Scale, RoadWheelZ * Scale));
		const FVector WheelWorld = GetActorLocation() + WheelOffset;

		FHitResult Hit;
		FCollisionQueryParams Params(TEXT("TankVehicleSpawnSnapRay"), /*bTraceComplex=*/false, this);
		Params.AddIgnoredActor(this);
		if (!World->LineTraceSingleByChannel(Hit, WheelWorld + Up, WheelWorld - Down, ECC_WorldStatic, Params))
		{
			Params.bTraceComplex = true;
			if (!World->LineTraceSingleByChannel(Hit, WheelWorld + Up, WheelWorld - Down, ECC_WorldStatic, Params))
			{
				++MissedCount;
				continue;
			}
		}

		++HitCount;
		GroundZ = FMath::Max(GroundZ, Hit.ImpactPoint.Z);
	}

	if (HitCount > 0)
	{
		const float TargetZ = GroundZ + SpawnGroundClearance;
		FVector Loc = GetActorLocation();
		const float OldZ = Loc.Z;
		Loc.Z = TargetZ;
		SetActorLocation(Loc, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

		UE_LOG(LogTank, Log, TEXT("[Vehicle] %s 出生落位：探测地面 z=%.1f（%d/12 探到，%d 空扫），车底 z=%.1f → %.1f（Δ=%.1f）"),
			*GetName(), GroundZ, HitCount, MissedCount, OldZ, TargetZ, TargetZ - OldZ);
	}
	else
	{
		UE_LOG(LogTank, Warning, TEXT("[Vehicle] %s 出生落位失败：只有 %d/12 个轮下探到地面（保持原高度，可能悬空）"),
			*GetName(), HitCount);
	}
}

// ============================================================================
// 驱动输入应用（坦克差速：W/S 同向、A/D 反向）
// ============================================================================
void ATankVehicle::ApplyDriveInput()
{
	if (!VehicleMovement || !VehicleMovement->PhysicsVehicleOutput().IsValid())
	{
		return;   // 载具仿真尚未建立（物理资产/物理场景未就绪）
	}

	const int32 NumWheels = VehicleMovement->GetNumWheels();
	if (NumWheels <= 0 || VehicleMovement->Wheels.Num() != NumWheels)
	{
		return;
	}

	float Throttle = 0.0f;
	float Steer = 0.0f;

	if (IsLocallyControlled())
	{
		Throttle = CurrentThrottleInput;
		Steer = CurrentSteerInput;
		if (!FMath::IsNearlyZero(DebugThrottle))
		{
			Throttle = DebugThrottle;
		}
		if (!FMath::IsNearlyZero(DebugSteer))
		{
			Steer = DebugSteer;
		}
		if (bInvertDriveDirection)
		{
			Throttle = -Throttle;
		}
		if (bInvertSteerDirection)
		{
			Steer = -Steer;
		}

		// 客户端向服务端上报驱动与炮塔朝向（限频 50Hz + 状态变化即时发送）
		if (!HasAuthority())
		{
			const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			const float SyncInterval = 1.0f / FMath::Max(NetInputSyncFrequency, 10.0f);
			const bool bInputChanged = !FMath::IsNearlyEqual(Throttle, LastSentThrottle, 0.02f)
				|| !FMath::IsNearlyEqual(Steer, LastSentSteer, 0.02f)
				|| !FMath::IsNearlyEqual(CurrentTurretYaw, LastSentTurretYaw, 0.5f)
				|| !FMath::IsNearlyEqual(CurrentPitch, LastSentGunPitch, 0.5f);

			if (Now - LastNetInputSyncTime >= SyncInterval || bInputChanged)
			{
				LastNetInputSyncTime = Now;
				LastSentThrottle = Throttle;
				LastSentSteer = Steer;
				LastSentTurretYaw = CurrentTurretYaw;
				LastSentGunPitch = CurrentPitch;
				ServerUpdateDriveInput(Throttle, Steer, CurrentTurretYaw, CurrentPitch);
			}
		}
	}
	else if (HasAuthority())
	{
		// 服务端处理远程客户端载具：采用上报的驱动指令（超时 0.5s 归零保护）
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (Now - LastDriveInputTime < 0.5f)
		{
			Throttle = ReplicatedThrottleInput;
			Steer = ReplicatedSteerInput;
		}
		else
		{
			Throttle = 0.0f;
			Steer = 0.0f;
		}
	}
	else
	{
		// 远端模拟代理（Simulated Proxy）：不直接施加轮力矩，由刚体物理移动复制驱动位姿
		return;
	}

	// 差速：右转 = 左履带加速 + 右履带减速
	const float LeftRatio = FMath::Clamp(Throttle + Steer, -1.0f, 1.0f);
	const float RightRatio = FMath::Clamp(Throttle - Steer, -1.0f, 1.0f);
	const bool bNoInput = FMath::IsNearlyZero(Throttle) && FMath::IsNearlyZero(Steer);

	// 告知引擎有人操作，防止激进休眠
	VehicleMovement->SetThrottleInput(Throttle);
	VehicleMovement->SetSteeringInput(Steer);
	VehicleMovement->SetBrakeInput(0.0f);

	for (int32 i = 0; i < NumWheels; ++i)
	{
		const bool bRightSide = (i < WheelsPerSide);
		const float SideRatio = bRightSide ? RightRatio : LeftRatio;
		const float Torque = SideRatio * MaxDriveTorque;
		VehicleMovement->SetDriveTorque(Torque, i);

		float Brake = 0.0f;
		if (bNoInput)
		{
			// 无操作时施加滑行阻力，迅速刹停杜绝溜车
			Brake = CoastBrakeTorque;
		}
		else if (FMath::IsNearlyZero(SideRatio) && !FMath::IsNearlyZero(Steer))
		{
			// 差速制动转向：当一侧驱动力矩归零且有转向输入时，对内侧履带施加制动作为转向支点
			Brake = CoastBrakeTorque * 1.5f;
		}
		VehicleMovement->SetBrakeTorque(Brake, i);
	}

	// 首次接地上报
	if (!bLoggedFirstGroundContact)
	{
		for (const UChaosVehicleWheel* Wheel : VehicleMovement->Wheels)
		{
			if (Wheel && !Wheel->IsInAir())
			{
				bLoggedFirstGroundContact = true;
				UE_LOG(LogTank, Log, TEXT("[Vehicle] %s 首次接地：悬挂行程 %.2f（物理链已通）"),
					*GetName(), Wheel->GetSuspensionOffset());
				break;
			}
		}
	}

	// 驱动力矩落地确认（限频 2 次/秒）
	if (!bNoInput)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (Now - LastDriveLogTime > 0.5f)
		{
			LastDriveLogTime = Now;
			const FWheelStatus& W0 = VehicleMovement->GetWheelState(0);
			const FBodyInstance* RootBodyInstance = VehicleMesh ? VehicleMesh->GetBodyInstance() : nullptr;
			UE_LOG(LogTank, Log, TEXT("[Vehicle] %s 施加驱动：左=%.2f 右=%.2f（%.0f Nm/轮 ×12）速度 %.0fcm/s | 仿真轮0：驱动=%.1f 刹车=%.1f 弹簧力=%.1f 接触=%d | 刚体质量=%.0fkg"),
				*GetName(), LeftRatio, RightRatio, MaxDriveTorque, GetVelocity().Size(),
				W0.DriveTorque, W0.BrakeTorque, W0.SpringForce, W0.bInContact ? 1 : 0,
				RootBodyInstance ? RootBodyInstance->GetBodyMass() : -1.0f);
		}
	}
}

// ============================================================================
// 负重轮视觉更新（转角与悬挂行程）
// ============================================================================
void ATankVehicle::UpdateWheelVisuals()
{
	if (!VehicleMovement || VehicleMovement->Wheels.Num() != RoadWheels.Num())
	{
		return;
	}
	if (!VehicleMovement->PhysicsVehicleOutput().IsValid())
	{
		return;
	}

	const float ScaleInv = 1.0f / FMath::Max(VehicleScale, 0.01f);
	for (int32 i = 0; i < RoadWheels.Num(); ++i)
	{
		UStaticMeshComponent* WheelComp = RoadWheels[i];
		UChaosVehicleWheel* Wheel = VehicleMovement->Wheels[i];
		if (!WheelComp || !Wheel)
		{
			continue;
		}

		const float SpinDeg = Wheel->GetRotationAngle();
		const float SuspensionOffset = Wheel->IsInAir() ? 0.0f : (-Wheel->GetSuspensionOffset() * ScaleInv);

		WheelComp->SetRelativeRotation(FRotator(SpinDeg, 0.0f, 0.0f));
		WheelComp->SetRelativeLocation(FVector(RoadSetups[i].X,
			RoadSetups[i].bRightSide ? RoadWheelY : -RoadWheelY,
			RoadWheelZ + SuspensionOffset));
	}
}

// ============================================================================
// 履带 UV 滚动更新
// ============================================================================
void ATankVehicle::UpdateTrackScroll(float DeltaTime)
{
	if (!TrackMaterialInstance || !VehicleMesh)
	{
		return;
	}

	const float ForwardSpeed = FVector::DotProduct(GetVelocity(), GetActorForwardVector());
	const float YawRateRad = VehicleMesh->GetPhysicsAngularVelocityInRadians().Z;
	const float HalfSpanWorld = TrackSpan * 0.5f * FMath::Max(VehicleScale, 0.01f);
	const float TrackSpeed = 0.5f * ((ForwardSpeed + YawRateRad * HalfSpanWorld) +
									 (ForwardSpeed - YawRateRad * HalfSpanWorld));

	TrackUVOffset += (TrackSpeed / (FMath::Max(1.0f, TrackUVSpeedScale) * FMath::Max(VehicleScale, 0.01f))) * DeltaTime;
	TrackMaterialInstance->SetScalarParameterValue(TrackOffsetParamName, TrackUVOffset);
}

// ============================================================================
// 调试状态输出与物理链路自检
// ============================================================================
void ATankVehicle::DrawDebugStatus()
{
	if (!VehicleMovement || !VehicleMovement->PhysicsVehicleOutput().IsValid())
	{
		return;
	}

	int32 ContactCount = 0;
	float MaxSuspension = 0.0f;
	for (int32 i = 0; i < VehicleMovement->Wheels.Num(); ++i)
	{
		if (const UChaosVehicleWheel* Wheel = VehicleMovement->Wheels[i])
		{
			if (Wheel->IsInAir() == false)
			{
				++ContactCount;
			}
			MaxSuspension = FMath::Max(MaxSuspension, FMath::Abs(Wheel->GetSuspensionOffset()));
		}
	}

	const FVector Vel = GetVelocity();
	const FRotator Rot = GetActorRotation();
	UE_LOG(LogTank, Verbose, TEXT("[Vehicle] %s 速度 %.0fcm/s 偏航 %.1f° 接地 %d/%d 悬挂最大行程 %.1f"),
		*GetName(), Vel.Size(), Rot.Yaw, ContactCount, VehicleMovement->Wheels.Num(), MaxSuspension);
}

void ATankVehicle::LogPhysicsSetupState(const TCHAR* Phase) const
{
	if (!VehicleMesh)
	{
		return;
	}

	const FBodyInstance* RootBody = VehicleMesh->GetBodyInstance();
	FString BodyDesc = TEXT("无根刚体");
	if (RootBody)
	{
		const UBodySetup* Setup = RootBody->GetBodySetup();
		const FBox LocalBounds = RootBody->GetBodyBoundsLocal();
		const FBox WorldBounds = RootBody->GetBodyBounds();
		FString BoxDesc = TEXT("无");
		if (Setup && Setup->AggGeom.BoxElems.Num() > 0)
		{
			const FKBoxElem& Box = Setup->AggGeom.BoxElems[0];
			BoxDesc = FString::Printf(TEXT("盒 %.0f×%.0f×%.0f 心=(%.0f,%.0f,%.0f)"),
				Box.X, Box.Y, Box.Z, Box.Center.X, Box.Center.Y, Box.Center.Z);
		}
		BodyDesc = FString::Printf(
			TEXT("形状数=%d 碰撞=%d 模拟=%d | 网格缩放=%.2f | %s | 本地包围盒 心=(%.0f,%.0f,%.0f) 半=(%.1f,%.1f,%.1f) | 世界包围盒 半=(%.1f,%.1f,%.1f)"),
			Setup ? Setup->AggGeom.GetElementCount() : -1,
			static_cast<int32>(RootBody->GetCollisionEnabled()),
			RootBody->IsInstanceSimulatingPhysics() ? 1 : 0,
			VehicleMesh->GetComponentScale().X,
			*BoxDesc,
			LocalBounds.GetCenter().X, LocalBounds.GetCenter().Y, LocalBounds.GetCenter().Z,
			LocalBounds.GetExtent().X, LocalBounds.GetExtent().Y, LocalBounds.GetExtent().Z,
			WorldBounds.GetExtent().X, WorldBounds.GetExtent().Y, WorldBounds.GetExtent().Z);
	}

	const FMatrix RootBoneMatrix = VehicleMesh->GetBoneMatrix(1);
	const FVector RootBoneLoc = RootBoneMatrix.GetOrigin();
	const float RootBoneScale = RootBoneMatrix.GetScaledAxis(EAxis::X).Size();
	const FVector ShapeTMScale = RootBody ? RootBody->Scale3D : FVector::ZeroVector;

	const bool bOutputValid = VehicleMovement && VehicleMovement->PhysicsVehicleOutput().IsValid();
	const int32 WheelNum = VehicleMovement ? VehicleMovement->Wheels.Num() : 0;
	const float WheelRadius = (bOutputValid && WheelNum > 0) ? VehicleMovement->Wheels[0]->GetWheelRadius() : 0.0f;

	UE_LOG(LogTank, Log, TEXT("[Vehicle] %s %s：物理状态=%d 刚体数=%d 模拟中=%d | %s | root骨 世界位置=(%.0f,%.0f,%.0f) 骨缩放=%.3f 刚体Scale3D=(%.2f,%.2f,%.2f) | 仿真输出=%d 轮数=%d 轮0半径=%.2f"),
		*GetName(), Phase,
		VehicleMesh->HasValidPhysicsState() ? 1 : 0,
		VehicleMesh->Bodies.Num(),
		VehicleMesh->IsAnySimulatingPhysics() ? 1 : 0,
		*BodyDesc,
		RootBoneLoc.X, RootBoneLoc.Y, RootBoneLoc.Z, RootBoneScale,
		ShapeTMScale.X, ShapeTMScale.Y, ShapeTMScale.Z,
		bOutputValid ? 1 : 0, WheelNum, WheelRadius);

	if (bOutputValid && WheelNum > 0)
	{
		int32 ContactCount = 0;
		for (int32 i = 0; i < WheelNum; ++i)
		{
			if (VehicleMovement->GetWheelState(i).bInContact)
			{
				++ContactCount;
			}
		}
		UE_LOG(LogTank, Log, TEXT("[Vehicle] %s %s：接地轮 %d/%d"), *GetName(), Phase, ContactCount, WheelNum);
	}
}

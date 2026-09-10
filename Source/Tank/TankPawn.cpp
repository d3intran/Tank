#include "TankPawn.h"
#include "Tank.h"
#include "TankProjectile.h"
#include "TankHealth.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	// 负重轮滚动半径（cm），均匀 z_span 69.8/2；Y 取 144 让轮盘端面退到履带
	// 外侧面（Y≈171）之后 ~3cm，避免旋转时与履带侧面 z-fighting 闪烁
	constexpr float RoadWheelRollRadius = 35.0f;
	constexpr float RoadWheelY = 144.0f;
	constexpr float RoadWheelZ = 41.75f;
}

ATankPawn::ATankPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 根碰撞盒：根据 1:1 真实尺寸（长7.6m, 宽3.5m, 高2.4m）
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	// 高度 240→236：盒底相对车体抬升 2cm 安全间隙——与齐平路面/地板共面时避免毫米级穿透导致扫掠卡死
	CollisionBox->SetBoxExtent(FVector(380.0f, 175.0f, 118.0f));
	// Pawn profile：Pawn 对 Pawn 默认 Block——FFA 坦克互挡靠它
	CollisionBox->SetCollisionProfileName(TEXT("Pawn"));
	CollisionBox->SetSimulatePhysics(false);
	RootComponent = CollisionBox;

	// 联机（M1 客户端权威移动）：Pawn 基类已 bReplicates=true；
	// 移动复制让服务器收到的位姿转发给其他端。
	// 本机位姿上报见 Tick 的 ServerSyncTransform——不开它，服务器永远停在出生点会把客户端拉回原地
	SetReplicateMovement(true);

	// 1b. 坦克血量组件（M0 挂载；M2 联机升级 Replicated）
	TankHealth = CreateDefaultSubobject<UTankHealth>(TEXT("TankHealth"));

	// 2. 车身底盘 Mesh（底部对齐碰撞盒底面 Z=-120）
	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	HullMesh->SetupAttachment(CollisionBox);
	HullMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -120.0f));
	HullMesh->SetRelativeScale3D(FVector::OneVector);
	HullMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> HullMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a_hull_body.ztz88a_hull_body"));
	if (HullMeshAsset.Succeeded())
	{
		HullMesh->SetStaticMesh(HullMeshAsset.Object);
	}

	// 3. 履带 Mesh（与底盘同原点同基准，严丝合缝包裹负重轮）
	TracksMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TracksMesh"));
	TracksMesh->SetupAttachment(HullMesh);
	TracksMesh->SetRelativeLocation(FVector::ZeroVector);
	TracksMesh->SetRelativeScale3D(FVector::OneVector);
	TracksMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TracksMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a_tracks_full.ztz88a_tracks_full"));
	if (TracksMeshAsset.Succeeded())
	{
		TracksMesh->SetStaticMesh(TracksMeshAsset.Object);
	}

	// 3b. 负重轮（每侧 6 个，位于履带环内）——由 split_tank_mesh.py 从车体网格拆出
	// 布局来自脚本输出：loc=(X, ±147.72, 41.75)，滚动半径 35cm（相对 TracksMesh 坐标系）
	struct FRoadWheelSetup
	{
		const TCHAR* MeshPath;
		float X;
		bool bRightSide;
	};
	const FRoadWheelSetup RoadSetups[] =
	{
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r0.ztz88a_road_r0"), -206.00f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r1.ztz88a_road_r1"), -131.00f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r2.ztz88a_road_r2"),  -56.00f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r3.ztz88a_road_r3"),   19.00f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r4.ztz88a_road_r4"),  107.50f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r5.ztz88a_road_r5"),  199.95f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l0.ztz88a_road_l0"), -206.00f, false },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l1.ztz88a_road_l1"), -131.00f, false },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l2.ztz88a_road_l2"),  -56.00f, false },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l3.ztz88a_road_l3"),   19.00f, false },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l4.ztz88a_road_l4"),  107.50f, false },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l5.ztz88a_road_l5"),  199.95f, false },
	};
	RoadWheels.Reserve(UE_ARRAY_COUNT(RoadSetups));
	RoadWheelAngles.Init(0.0f, UE_ARRAY_COUNT(RoadSetups));
	for (int32 i = 0; i < UE_ARRAY_COUNT(RoadSetups); ++i)
	{
		UStaticMeshComponent* WheelComp = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("RoadWheel%d"), i));
		WheelComp->SetupAttachment(TracksMesh);
		WheelComp->SetRelativeLocation(FVector(RoadSetups[i].X,
			RoadSetups[i].bRightSide ? RoadWheelY : -RoadWheelY, RoadWheelZ));
		WheelComp->SetCollisionProfileName(TEXT("NoCollision"));

		ConstructorHelpers::FObjectFinder<UStaticMesh> WheelMeshAsset(RoadSetups[i].MeshPath);
		if (WheelMeshAsset.Succeeded())
		{
			WheelComp->SetStaticMesh(WheelMeshAsset.Object);
		}
		RoadWheels.Add(WheelComp);
	}

	// 3c. 端轮说明：主动轮/诱导轮盘体在车体网格内、被履带包绕覆盖，不做独立
	// 旋转组件（此前尝试拆出旋转端轮会带出履带弧块，见 split_tank_mesh.py 注释）

	// 4. 炮塔水平旋转轴心 (挂在车身上，坐标：X=-2.5, Y=0, Z=145.5)
	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(HullMesh);
	TurretPivot->SetRelativeLocation(FVector(-2.5f, 0.0f, 145.5f));
	TurretPivot->SetRelativeScale3D(FVector::OneVector);

	// 5. 炮塔 Mesh（完整包含铸造炮塔、五角星侧装甲裙框、高射机枪与指挥塔）
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(TurretPivot);
	TurretMesh->SetRelativeLocation(FVector::ZeroVector);
	TurretMesh->SetRelativeRotation(FRotator::ZeroRotator);
	TurretMesh->SetRelativeScale3D(FVector::OneVector);
	TurretMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TurretMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a-turret.ztz88a-turret"));
	if (TurretMeshAsset.Succeeded())
	{
		TurretMesh->SetStaticMesh(TurretMeshAsset.Object);
	}

	// 6. 主炮俯仰轴心 (挂在炮塔轴心上，相对于炮塔基准点坐标：X=170.0, Y=0, Z=31.0)
	GunPivot = CreateDefaultSubobject<USceneComponent>(TEXT("GunPivot"));
	GunPivot->SetupAttachment(TurretPivot);
	GunPivot->SetRelativeLocation(FVector(170.0f, 0.0f, 31.0f));
	GunPivot->SetRelativeScale3D(FVector::OneVector);

	// 7. 主炮 Mesh
	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(GunPivot);
	GunMesh->SetRelativeLocation(FVector::ZeroVector);
	GunMesh->SetRelativeRotation(FRotator::ZeroRotator);
	GunMesh->SetRelativeScale3D(FVector::OneVector);
	GunMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> GunMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a-gun.ztz88a-gun"));
	if (GunMeshAsset.Succeeded())
	{
		GunMesh->SetStaticMesh(GunMeshAsset.Object);
	}

	// 8. 现代商业标杆视口弹簧臂（锁定舒适俯视角，彻底杜绝翻转眩晕，仅绕车体水平回旋）
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CollisionBox);
	SpringArm->TargetArmLength = 950.0f;
	SpringArm->SetRelativeLocation(FVector(-50.0f, 0.0f, 220.0f));
	SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = true;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true; // 核心修复：继承底盘偏航，WA转向时视口自然跟随车身切入弯道！
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.0f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 8.0f; // 优雅的旋转滞后平滑，营造重型载具厚重感

	// 9. 摄像机
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	AutoPossessPlayer = EAutoReceiveInput::Player0;
	ProjectileClass = ATankProjectile::StaticClass();

	// 10. Enhanced Input：动作与映射上下文资产位于 /Game/tank/inputs/（MCP 创建）
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCAsset(TEXT("/Game/tank/inputs/IMC_Tank.IMC_Tank"));
	if (IMCAsset.Succeeded())
	{
		DefaultMappingContext = IMCAsset.Object;
	}

	const std::pair<const TCHAR*, TObjectPtr<UInputAction>*> InputAssets[] =
	{
		{ TEXT("/Game/tank/inputs/IA_MoveForward.IA_MoveForward"), &MoveForwardAction },
		{ TEXT("/Game/tank/inputs/IA_MoveBackward.IA_MoveBackward"), &MoveBackwardAction },
		{ TEXT("/Game/tank/inputs/IA_TurnRight.IA_TurnRight"), &TurnRightAction },
		{ TEXT("/Game/tank/inputs/IA_TurnLeft.IA_TurnLeft"), &TurnLeftAction },
		{ TEXT("/Game/tank/inputs/IA_TurretCW.IA_TurretCW"), &TurretCWAction },
		{ TEXT("/Game/tank/inputs/IA_TurretCCW.IA_TurretCCW"), &TurretCCWAction },
		{ TEXT("/Game/tank/inputs/IA_CameraYaw.IA_CameraYaw"), &CameraYawAction },
		{ TEXT("/Game/tank/inputs/IA_GunPitch.IA_GunPitch"), &GunPitchAction },
		{ TEXT("/Game/tank/inputs/IA_Fire.IA_Fire"), &FireAction },
	};
	for (const auto& InputAsset : InputAssets)
	{
		ConstructorHelpers::FObjectFinder<UInputAction> ActionAsset(InputAsset.first);
		if (ActionAsset.Succeeded())
		{
			*InputAsset.second = ActionAsset.Object;
		}
	}
}

void ATankPawn::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	// 核心架构自愈机制：强制保证编辑期与运行期拓扑及相对位姿一致性
	if (TurretPivot && GunPivot)
	{
		GunPivot->AttachToComponent(TurretPivot, FAttachmentTransformRules::KeepRelativeTransform);
		GunPivot->SetRelativeLocation(FVector(170.0f, 0.0f, 31.0f));
		GunPivot->SetRelativeScale3D(FVector::OneVector);
	}

	if (GunMesh && GunPivot)
	{
		GunMesh->AttachToComponent(GunPivot, FAttachmentTransformRules::KeepRelativeTransform);
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
		GunMesh->SetRelativeScale3D(FVector::OneVector);
	}
}

void ATankPawn::BeginPlay()
{
	Super::BeginPlay();

	// 1. 初始化动态履带材质实例
	if (TracksMesh)
	{
		if (TracksMesh->GetNumMaterials() > 0)
		{
			TrackMaterialInstance = TracksMesh->CreateAndSetMaterialInstanceDynamic(0);
		}
		else
		{
			UE_LOG(LogTank, Warning, TEXT("TracksMesh has no materials assigned. Track UV animation disabled."));
		}
	}

	// 2. 初始化相机朝向与火炮仰角
	CameraRelativeYaw = 0.0f;
	DesiredGunPitch = 0.0f;
	CurrentPitch = 0.0f;

	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, CameraRelativeYaw, 0.0f));
	}

	UE_LOG(LogTank, Log, TEXT("TankPawn initialized with Vehicle-Relative Camera and Direct Gun Elevation."));

	AddDefaultMappingContext();
}

void ATankPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// 服务器生成坦克的顺序是 Spawn（触发 BeginPlay，此时 Controller 为空）→ Possess。
	// 映射上下文若只在 BeginPlay 挂，主机（服务器本地玩家）永远挂不上、输入失灵
	AddDefaultMappingContext();
}

void ATankPawn::UnPossessed()
{
	Super::UnPossessed();
	// M1 联调：抓 PIE 多开下主机占有被清空的时机（t=世界秒）
	UE_LOG(LogTank, Warning, TEXT("[Input] %s 被 UnPossessed！t=%.1f"), *GetName(), GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f);
}

void ATankPawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	// 客户端的 Possess 不会跨网调用，用 Controller 复制回调兜底
	AddDefaultMappingContext();
}

void ATankPawn::AddDefaultMappingContext()
{
	if (bMappingContextAdded || !DefaultMappingContext)
	{
		return;
	}
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		UE_LOG(LogTank, Warning, TEXT("[Input] %s 挂上下文失败：无 PC（Role=%d）"), *GetName(), (int32)GetLocalRole());
		return;
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		UE_LOG(LogTank, Warning, TEXT("[Input] %s 挂上下文失败：无 LocalPlayer 子系统"), *GetName());
		return;
	}
	Subsystem->AddMappingContext(DefaultMappingContext, 0);
	bMappingContextAdded = true;
	UE_LOG(LogTank, Log, TEXT("[Input] %s 上下文已挂载（Role=%d）"), *GetName(), (int32)GetLocalRole());
}

void ATankPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// M1 客户端权威移动同步：本机客户端模拟 → 50Hz 上报服务器 →
	// 服务器 SetActorLocationAndRotation 后经移动复制转发其他端。
	// 主机端（Authority）直接本地模拟，无需上报
	if (IsLocallyControlled() && !HasAuthority())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastTransformSyncTime >= TransformSyncInterval)
		{
			LastTransformSyncTime = Now;
			ServerSyncTransform(GetActorLocation(), GetActorRotation());
		}
	}

	// 0.9 被推位移平滑消化：推挤 RPC 按 推动 方帧率到达，节奏差会造成跳帧；
	// 累积进 PendingPushOffset 后按恒定速度消耗（sweep 撞墙即弃剩余量，推动方下帧的推挤会继续跟进）
	if (!PendingPushOffset.IsNearlyZero())
	{
		const FVector Step = PendingPushOffset.GetClampedToMaxSize(PushConsumeSpeed * DeltaTime);
		FHitResult PushHit;
		SetActorLocation(GetActorLocation() + Step, true, &PushHit);
		PendingPushOffset = PushHit.bBlockingHit ? FVector::ZeroVector : PendingPushOffset - Step;
	}

	// 1. WASD 前后行进
	if (!FMath::IsNearlyZero(CurrentMoveInput))
	{
		const FVector MoveDelta = FVector(CurrentMoveInput * MoveSpeed * DeltaTime, 0.0f, 0.0f);
		FHitResult Hit;
		AddActorLocalOffset(MoveDelta, true, &Hit);

		// M1.5 挤压推进：本机扫掠被对方坦克挡住 → 沿本机推进方向把对方顶开
		if (Hit.bBlockingHit)
		{
			if (ATankPawn* HitTank = Cast<ATankPawn>(Hit.GetActor()))
			{
				TryPushTank(HitTank, GetActorRotation().RotateVector(MoveDelta) * PushStrength);
			}
		}

		if (TrackMaterialInstance)
		{
			TrackUVOffset += CurrentMoveInput * (MoveSpeed / FMath::Max(1.0f, TrackUVSpeedScale)) * DeltaTime;
			TrackMaterialInstance->SetScalarParameterValue(TrackOffsetParamName, TrackUVOffset);
		}
	}

	// 2. WASD 原地差速掉头（坦克约定：A/D 恒定转向车体，与前后进无关，倒车不反向）
	if (!FMath::IsNearlyZero(CurrentTurnInput))
	{
		const FRotator TurnDelta = FRotator(0.0f, CurrentTurnInput * TurnSpeed * DeltaTime, 0.0f);
		AddActorLocalRotation(TurnDelta, true);

		if (TrackMaterialInstance && FMath::IsNearlyZero(CurrentMoveInput))
		{
			TrackUVOffset += CurrentTurnInput * (TurnSpeed / FMath::Max(1.0f, TrackUVSpeedScale * 0.1f)) * DeltaTime;
			TrackMaterialInstance->SetScalarParameterValue(TrackOffsetParamName, TrackUVOffset);
		}
	}

	// 3. 视口跟随：继承车身偏航并叠加相对偏航角 CameraRelativeYaw，Pitch锁定在舒适俯角
	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, CameraRelativeYaw, 0.0f));
	}

	// 4. 直线行驶自动回正（如启用）
	if (bEnableAutoCenter && !FMath::IsNearlyZero(CurrentMoveInput) && FMath::IsNearlyZero(CurrentTurnInput))
	{
		CameraRelativeYaw = FMath::FInterpTo(CameraRelativeYaw, 0.0f, DeltaTime, AutoCenterSpeed);
	}

	// 5. 炮塔水平电驱伺服追踪（追赶玩家视线相对偏航角 CameraRelativeYaw）
	if (!FMath::IsNearlyZero(CurrentTurretRotateInput))
	{
		CameraRelativeYaw = FRotator::NormalizeAxis(CameraRelativeYaw + CurrentTurretRotateInput * TurretRotateSpeed * DeltaTime);
	}
	CurrentTurretYaw = FMath::FixedTurn(CurrentTurretYaw, CameraRelativeYaw, TurretRotateSpeed * DeltaTime);

	// 6. 主炮垂直电驱高低机平滑追踪（平滑向鼠标纵向指定的 DesiredGunPitch 靠拢）
	if (!FMath::IsNearlyZero(CurrentPitchInput))
	{
		DesiredGunPitch = FMath::Clamp(DesiredGunPitch + CurrentPitchInput * PitchSpeed * DeltaTime, MinPitch, MaxPitch);
	}
	CurrentPitch = FMath::FInterpConstantTo(CurrentPitch, DesiredGunPitch, DeltaTime, PitchSpeed);

	if (TurretPivot)
	{
		TurretPivot->SetRelativeRotation(FRotator(0.0f, CurrentTurretYaw, 0.0f));
	}
	if (GunPivot)
	{
		GunPivot->SetRelativeRotation(FRotator(CurrentPitch, 0.0f, 0.0f));
	}

	// 7. 主炮后坐力渐进平滑复位
	if (GunMesh)
	{
		if (CurrentRecoilOffset < -0.01f)
		{
			CurrentRecoilOffset = FMath::FInterpTo(CurrentRecoilOffset, 0.0f, DeltaTime, RecoilRecoverySpeed);
		}
		else
		{
			CurrentRecoilOffset = 0.0f;
		}
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
	}

	// 8. 负重轮差速旋转（+Yaw 为右转：右履带减速、左履带加速；轮角速度=履带线速度/滚动半径）
	if (!FMath::IsNearlyZero(CurrentMoveInput) || !FMath::IsNearlyZero(CurrentTurnInput))
	{
		const float YawRateRad = FMath::DegreesToRadians(CurrentTurnInput * TurnSpeed);
		const float HalfSpan = FMath::Max(1.0f, TrackSpan) * 0.5f;
		const float TrackSpeedLeft = CurrentMoveInput * MoveSpeed + YawRateRad * HalfSpan;
		const float TrackSpeedRight = CurrentMoveInput * MoveSpeed - YawRateRad * HalfSpan;
		const float SpinSign = bInvertWheelSpin ? -1.0f : 1.0f;

		// 负重轮：0-5 为右侧、6-11 为左侧，与所在侧履带线速度一致
		for (int32 i = 0; i < RoadWheels.Num(); ++i)
		{
			const float SideSpeed = (i < 6) ? TrackSpeedRight : TrackSpeedLeft;
			RoadWheelAngles[i] = FMath::Fmod(RoadWheelAngles[i] + SpinSign * FMath::RadiansToDegrees(SideSpeed / RoadWheelRollRadius) * DeltaTime, 360.0f);
			if (RoadWheels[i])
			{
				RoadWheels[i]->SetRelativeRotation(FRotator(RoadWheelAngles[i], 0.0f, 0.0f));
			}
		}
	}

	// 9. 真实弹道指向与射击点（从炮口沿炮管轴线探测，鼠标纵向调节时清晰上下位移）
	if (bDrawAimDebug && GetWorld() && GunMesh)
	{
		const FVector MuzzleLoc = GunMesh->GetComponentTransform().TransformPosition(FVector(MuzzleForwardOffset, 0.0f, 0.0f));
		const FVector GunForward = GunMesh->GetForwardVector();
		const FVector GunTraceEnd = MuzzleLoc + GunForward * MaxAimDistance;

		FHitResult GunHit;
		FCollisionQueryParams GunQueryParams(TEXT("TankGunTrace"), false, this);
		GunQueryParams.AddIgnoredActor(this);

		FVector ActualAimPoint = GunTraceEnd;
		if (GetWorld()->LineTraceSingleByChannel(GunHit, MuzzleLoc, GunTraceEnd, ECC_Visibility, GunQueryParams))
		{
			ActualAimPoint = GunHit.ImpactPoint;
		}

		// 绘制实际射击着弹点与主炮指向射线
		DrawDebugSphere(GetWorld(), ActualAimPoint, 35.0f, 16, FColor::Yellow, false, -1.0f, 0, 2.5f);
		DrawDebugLine(GetWorld(), MuzzleLoc, MuzzleLoc + GunForward * 1200.0f, FColor::Orange, false, -1.0f, 0, 2.0f);
	}

	CurrentMoveInput = 0.0f;
	CurrentTurnInput = 0.0f;
	CurrentTurretRotateInput = 0.0f;
	CurrentPitchInput = 0.0f;
}

void ATankPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTank, Error, TEXT("Missing EnhancedInputComponent — check DefaultInput.ini DefaultInputComponentClass."));
		return;
	}

	EnhancedInput->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &ATankPawn::MoveForward);
	EnhancedInput->BindAction(MoveBackwardAction, ETriggerEvent::Triggered, this, &ATankPawn::MoveBackward);
	EnhancedInput->BindAction(TurnRightAction, ETriggerEvent::Triggered, this, &ATankPawn::TurnRight);
	EnhancedInput->BindAction(TurnLeftAction, ETriggerEvent::Triggered, this, &ATankPawn::TurnLeft);
	EnhancedInput->BindAction(TurretCWAction, ETriggerEvent::Triggered, this, &ATankPawn::TurretCW);
	EnhancedInput->BindAction(TurretCCWAction, ETriggerEvent::Triggered, this, &ATankPawn::TurretCCW);
	EnhancedInput->BindAction(CameraYawAction, ETriggerEvent::Triggered, this, &ATankPawn::OrbitCamera);
	EnhancedInput->BindAction(GunPitchAction, ETriggerEvent::Triggered, this, &ATankPawn::ElevateGun);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &ATankPawn::Fire);
}

void ATankPawn::MoveForward()
{
	UE_LOG(LogTank, Log, TEXT("[Input] %s MoveForward 触发（Role=%d）"), *GetName(), (int32)GetLocalRole());
	CurrentMoveInput = 1.0f;
}

void ATankPawn::MoveBackward()
{
	CurrentMoveInput = -1.0f;
}

void ATankPawn::TurnRight()
{
	CurrentTurnInput = 1.0f;
}

void ATankPawn::TurnLeft()
{
	CurrentTurnInput = -1.0f;
}

void ATankPawn::TurretCW()
{
	CurrentTurretRotateInput = 1.0f;
}

void ATankPawn::TurretCCW()
{
	CurrentTurretRotateInput = -1.0f;
}

void ATankPawn::OrbitCamera(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (!FMath::IsNearlyZero(Axis))
	{
		CameraRelativeYaw = FRotator::NormalizeAxis(CameraRelativeYaw + Axis * CameraSensitivityX);
	}
}

void ATankPawn::ElevateGun(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (!FMath::IsNearlyZero(Axis))
	{
		// 鼠标向上推 -> 仰角增大(向上抬)；鼠标向下拉 -> 仰角减小(向下压)
		const float Direction = bInvertPitch ? -1.0f : 1.0f;
		DesiredGunPitch = FMath::Clamp(DesiredGunPitch + Axis * PitchSensitivity * Direction, MinPitch, MaxPitch);
	}
}

void ATankPawn::Fire()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastFireTime < FireCooldown)
	{
		UE_LOG(LogTank, Verbose, TEXT("[Tank] Gun reloading: %.2f / %.2fs"), (CurrentTime - LastFireTime), FireCooldown);
		return;
	}

	LastFireTime = CurrentTime;

	// 1. 触发主炮液压后坐力冲压（本机表现即时预测，其他端由 MulticastFireFX 补齐）
	CurrentRecoilOffset = RecoilDistance;
	if (GunMesh)
	{
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
	}

	// 2. 炮口世界位姿（炮塔俯仰是本机状态，服务器不知道——随 RPC 上报，灰盒接受其作弊面）
	const FVector MuzzleLoc = GunMesh ? GunMesh->GetComponentTransform().TransformPosition(FVector(MuzzleForwardOffset, 0.0f, 0.0f)) : GetActorLocation();
	const FVector AimDir = GunMesh ? GunMesh->GetComponentRotation().Vector() : GetActorRotation().Vector();

	// 3. 炮弹生成服务器独占（双端生成会双重伤害）
	if (HasAuthority())
	{
		ExecuteFire(MuzzleLoc, AimDir);
	}
	else
	{
		ServerFire(MuzzleLoc, AimDir);
	}

	// 4. 炮口瞬时开火闪光/示踪线（本机即时，其他端由 Multicast 补）
	DrawDebugLine(World, MuzzleLoc, MuzzleLoc + AimDir * 3000.0f, FColor::Yellow, false, 0.15f, 0, 4.0f);
}

void ATankPawn::ExecuteFire(const FVector& MuzzleLoc, const FVector& AimDir)
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* ClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ATankProjectile::StaticClass();
	World->SpawnActor<ATankProjectile>(ClassToSpawn, MuzzleLoc, AimDir.Rotation(), SpawnParams);

	UE_LOG(LogTank, Log, TEXT("[Battle] %s 开火 @ (%.0f,%.0f,%.0f)"),
		*GetName(), MuzzleLoc.X, MuzzleLoc.Y, MuzzleLoc.Z);
}

bool ATankPawn::ServerFire_Validate(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir)
{
	const FVector Loc(MuzzleLoc);
	const FVector Dir(AimDir);
	return !Loc.ContainsNaN() && !Dir.ContainsNaN() && !Dir.IsNearlyZero();
}

void ATankPawn::ServerFire_Implementation(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir)
{
	// 服务器侧宽松射速限制（容忍网络延迟下的合法连发，拦截无脑刷弹）
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < FireCooldown * 0.5f)
	{
		return;
	}
	LastFireTime = Now;

	ExecuteFire(FVector(MuzzleLoc), FVector(AimDir));
	MulticastFireFX(MuzzleLoc, AimDir);
}

void ATankPawn::MulticastFireFX_Implementation(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir)
{
	// 远端后坐力动画（所有者本机已在 Fire() 里预测过，重复置位无害）
	CurrentRecoilOffset = RecoilDistance;
	if (GunMesh)
	{
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
	}
	if (UWorld* World = GetWorld())
	{
		DrawDebugLine(World, MuzzleLoc, FVector(MuzzleLoc) + FVector(AimDir) * 3000.0f, FColor::Yellow, false, 0.15f, 0, 4.0f);
	}
}

void ATankPawn::MulticastDeathFX_Implementation(FVector_NetQuantize100 DeathLoc)
{
	if (UWorld* World = GetWorld())
	{
		// 灰盒爆炸表达：橙红双球驻留 2s（P4 换 Niagara）
		DrawDebugSphere(World, FVector(DeathLoc), 250.0f, 16, FColor::Orange, true, 2.0f, 0, 6.0f);
		DrawDebugSphere(World, FVector(DeathLoc) + FVector(0, 0, 120.0f), 150.0f, 16, FColor::Red, true, 2.0f, 0, 4.0f);
	}
}

float ATankPawn::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// 伤害只在服务器结算（炮弹命中判定服务器独占，客户端弹体 OnHit 已被门挡掉）
	if (!HasAuthority() || DamageAmount <= 0.0f || !TankHealth || TankHealth->IsDepleted())
	{
		return 0.0f;
	}
	const float Remaining = TankHealth->ApplyDamage(DamageAmount);
	UE_LOG(LogTank, Log, TEXT("[Battle] %s 遭受 %.0f 伤害（余 %.0f/%.0f，来源 %s）"),
		*GetName(), DamageAmount, Remaining, TankHealth->GetMaxHealth(),
		DamageCauser ? *DamageCauser->GetName() : TEXT("?"));

	if (TankHealth->IsDepleted())
	{
		HandleDeath(EventInstigator);
	}
	return DamageAmount;
}

void ATankPawn::HandleDeath(AController* Killer)
{
	const FVector DeathLoc = GetActorLocation();
	UE_LOG(LogTank, Warning, TEXT("[Battle] %s 被击毁（击杀者 %s），2s 后重生"),
		*GetName(), Killer ? *Killer->GetName() : TEXT("?"));

	// 先广播表现再销毁；SetLifeSpan 留出 Multicast 的发送窗口，销毁后由占有自愈巡检在 2s 内补发新坦克
	MulticastDeathFX(DeathLoc);
	SetLifeSpan(0.2f);
}

void ATankPawn::ServerSyncTransform_Implementation(const FVector_NetQuantize100& Location, const FRotator& NetRotation)
{
	// 信任客户端位姿（M1 无反作弊，见计划书）。不走 sweep：客户端本地已做过碰撞解析，
	// 服务器再 sweep 会因时序差拒绝合法移动；物理状态重置避免与瞬时大步距冲突
	SetActorLocationAndRotation(FVector(Location), NetRotation, false, nullptr, ETeleportType::ResetPhysics);
}

bool ATankPawn::ServerSyncTransform_Validate(const FVector_NetQuantize100& Location, const FRotator& NetRotation)
{
	const FVector Loc(Location);
	return !Loc.ContainsNaN() && !NetRotation.ContainsNaN();
}

// ==================== M1.5 挤压推进 ====================

void ATankPawn::TryPushTank(ATankPawn* HitTank, const FVector& PushDelta)
{
	if (HasAuthority())
	{
		ExecuteTankPush(HitTank, PushDelta);
	}
	else
	{
		ServerPushTank(HitTank, PushDelta);
	}
}

bool ATankPawn::ServerPushTank_Validate(ATankPawn* PushedTank, FVector_NetQuantize10 PushDelta)
{
	// 宽松校验：拒绝空引用与物理上不可能的单帧推挤量（500cm ≫ 600cm/s 速度在任何合法 dt 下都到不了）
	const FVector Delta(PushDelta);
	return IsValid(PushedTank) && !Delta.ContainsNaN() && Delta.Size() < 500.0f;
}

void ATankPawn::ServerPushTank_Implementation(ATankPawn* PushedTank, FVector_NetQuantize10 PushDelta)
{
	ExecuteTankPush(PushedTank, FVector(PushDelta));
}

void ATankPawn::ClientApplyPush_Implementation(FVector_NetQuantize10 PushDelta)
{
	// Client RPC 只投递给被推端的所有者连接；防御性跳过服务器本机执行路径
	if (!HasAuthority())
	{
		ApplyPushDelta(FVector(PushDelta));
	}
}

void ATankPawn::ExecuteTankPush(ATankPawn* PushedTank, const FVector& PushDelta)
{
	if (!PushedTank || PushedTank == this || PushDelta.IsNearlyZero())
	{
		return;
	}
	// 注意：Listen 服务器上所有复制 Pawn 的 HasAuthority() 都是 true，不能用 HasAuthority 区分归属！
	// IsLocallyControlled 在服务器上仅主机自有坦克为 true
	if (PushedTank->IsLocallyControlled())
	{
		// 被推坦克属于主机：主机即权威，直接应用
		PushedTank->ApplyPushDelta(PushDelta);
	}
	else
	{
		// 被推坦克由客户端权威：通知其所有者本地应用，随其常规位姿上报自动收敛到服务器（无位置打架）
		PushedTank->ClientApplyPush(PushDelta);
	}
}

void ATankPawn::ApplyPushDelta(const FVector& PushDelta)
{
	// 不瞬移：累积进平滑消化队列（上限 300cm 防积压），Tick 按 PushConsumeSpeed 恒速消耗
	PendingPushOffset = (PendingPushOffset + PushDelta).GetClampedToMaxSize(300.0f);
}

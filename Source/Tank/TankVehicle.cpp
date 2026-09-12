#include "TankVehicle.h"
#include "TankVehicleLayout.h"

#include "Camera/CameraComponent.h"
#include "ChaosVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Tank.h"
#include "TankHealth.h"
#include "TankProjectile.h"
#include "TankVehicleWheel.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

using namespace TankVehicleLayout;

// ============================================================================
// 构造函数：组件层级拓扑与资产引用绑定
// ============================================================================
ATankVehicle::ATankVehicle()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 根组件 = 骨骼网格（Chaos 载具的刚体本体，必须 Simulate Physics）
	VehicleMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VehicleMesh"));
	VehicleMesh->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	VehicleMesh->BodyInstance.bSimulatePhysics = true;
	VehicleMesh->BodyInstance.bNotifyRigidBodyCollision = true;
	VehicleMesh->BodyInstance.bUseCCD = true;
	VehicleMesh->BodyInstance.LinearDamping = 0.2f;
	VehicleMesh->BodyInstance.AngularDamping = 1.5f;
	VehicleMesh->SetGenerateOverlapEvents(true);
	VehicleMesh->SetCanEverAffectNavigation(false);
	VehicleMesh->SetRelativeScale3D(FVector(VehicleScale));
	RootComponent = VehicleMesh;
	VehicleMesh->SetIsReplicated(true);

	// 联机配置
	bReplicates = true;
	SetReplicateMovement(true);
	NetUpdateFrequency = 60.0f;
	MinNetUpdateFrequency = 30.0f;
	NetPriority = 3.0f;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(SkeletalMeshPath);
	if (MeshAsset.Succeeded())
	{
		VehicleMesh->SetSkeletalMeshAsset(MeshAsset.Object);
	}
	else
	{
		UE_LOG(LogTank, Error, TEXT("[Vehicle] 骨骼网格加载失败：%s"), SkeletalMeshPath);
	}

	// 材质槽覆盖：槽 0 = 车体 mat_61，槽 1 = 履带 mat_60
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HullMaterial(TEXT("/Game/tank/ztz-88a/mat_61.mat_61"));
	if (HullMaterial.Succeeded())
	{
		VehicleMesh->SetMaterial(0, HullMaterial.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrackMaterial(TEXT("/Game/tank/ztz-88a/mat_60.mat_60"));
	if (TrackMaterial.Succeeded())
	{
		VehicleMesh->SetMaterial(1, TrackMaterial.Object);
	}

	TankHealth = CreateDefaultSubobject<UTankHealth>(TEXT("TankHealth"));

	// 2. 载具运动组件：12 个物理轮（骨骼名 wheel_r0-5 / wheel_l0-5）
	VehicleMovement = CreateDefaultSubobject<UChaosWheeledVehicleMovementComponent>(TEXT("VehicleMovement"));
	VehicleMovement->SetIsReplicated(true);
	VehicleMovement->UpdatedComponent = VehicleMesh;
	VehicleMovement->Mass = VehicleMass;
	VehicleMovement->bMechanicalSimEnabled = false;   // 力矩由 ApplyDriveInput 按轮直接给
	VehicleMovement->bEnableCenterOfMassOverride = true;
	VehicleMovement->CenterOfMassOverride = FVector(0.0f, 0.0f, CenterOfMassZ * VehicleScale);
	VehicleMovement->ChassisWidth = ChassisBoxHalfY * 2.0f * VehicleScale;
	VehicleMovement->ChassisHeight = ChassisBoxHalfZ * 2.0f * VehicleScale;
	MaxDriveTorque = 1200.0f;
	CoastBrakeTorque = 400.0f;
	VehicleMovement->WheelSetups.SetNum(UE_ARRAY_COUNT(RoadSetups));
	for (int32 i = 0; i < UE_ARRAY_COUNT(RoadSetups); ++i)
	{
		VehicleMovement->WheelSetups[i].WheelClass = UTankVehicleWheel::StaticClass();
		VehicleMovement->WheelSetups[i].BoneName = (RoadSetups[i].bRightSide)
			? FName(*FString::Printf(TEXT("wheel_r%d"), i))
			: FName(*FString::Printf(TEXT("wheel_l%d"), i - WheelsPerSide));
	}

	// 3. 视觉负重轮：12 个静态网格
	RoadWheels.Reserve(UE_ARRAY_COUNT(RoadSetups));
	for (int32 i = 0; i < UE_ARRAY_COUNT(RoadSetups); ++i)
	{
		UStaticMeshComponent* WheelComp = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("RoadWheel%d"), i));
		WheelComp->SetupAttachment(VehicleMesh, FName(TEXT("root")));
		WheelComp->SetRelativeLocation(FVector(RoadSetups[i].X,
			RoadSetups[i].bRightSide ? RoadWheelY : -RoadWheelY, RoadWheelZ));
		WheelComp->SetCollisionProfileName(TEXT("NoCollision"));
		WheelComp->SetGenerateOverlapEvents(false);

		ConstructorHelpers::FObjectFinder<UStaticMesh> WheelMeshAsset(RoadSetups[i].MeshPath);
		if (WheelMeshAsset.Succeeded())
		{
			WheelComp->SetStaticMesh(WheelMeshAsset.Object);
		}
		RoadWheels.Add(WheelComp);
	}

	// 4. 炮塔水平旋转轴心
	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(VehicleMesh, FName(TEXT("root")));
	TurretPivot->SetRelativeLocation(FVector(-2.5f, 0.0f, 145.5f));

	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(TurretPivot);
	TurretMesh->SetCollisionProfileName(TEXT("NoCollision"));
	TurretMesh->SetGenerateOverlapEvents(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TurretMeshAsset(TurretMeshPath);
	if (TurretMeshAsset.Succeeded())
	{
		TurretMesh->SetStaticMesh(TurretMeshAsset.Object);
	}

	// 5. 主炮俯仰轴心
	GunPivot = CreateDefaultSubobject<USceneComponent>(TEXT("GunPivot"));
	GunPivot->SetupAttachment(TurretPivot);
	GunPivot->SetRelativeLocation(FVector(170.0f, 0.0f, 31.0f));

	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(GunPivot);
	GunMesh->SetCollisionProfileName(TEXT("NoCollision"));
	GunMesh->SetGenerateOverlapEvents(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GunMeshAsset(GunMeshPath);
	if (GunMeshAsset.Succeeded())
	{
		GunMesh->SetStaticMesh(GunMeshAsset.Object);
	}

	// 6. 相机弹簧臂与相机
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(VehicleMesh, FName(TEXT("root")));
	SpringArm->TargetArmLength = CameraArmLength * VehicleScale;
	SpringArm->SetRelativeLocation(FVector(-50.0f, 0.0f, 220.0f + ChassisBoxCenterZ));
	SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = true;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.0f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 8.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;
	ProjectileClass = ATankProjectile::StaticClass();

	// 7. Enhanced Input 动作资产查找
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

// ============================================================================
// 生命周期与组件初始化
// ============================================================================
void ATankVehicle::PostInitProperties()
{
	Super::PostInitProperties();
}

void ATankVehicle::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	EnsureChassisPhysicsBox(VehicleMesh);

	if (VehicleMesh)
	{
		VehicleMesh->SetSimulatePhysics(true);
	}
}

void ATankVehicle::PostActorCreated()
{
	Super::PostActorCreated();

	if (HasAuthority())
	{
		SnapToGroundOnSpawn();
	}
}

void ATankVehicle::BeginPlay()
{
	Super::BeginPlay();

	EnsureChassisPhysicsBox(VehicleMesh);

	if (HasAuthority())
	{
		SnapToGroundOnSpawn();
	}

	// 履带动态材质
	if (VehicleMesh)
	{
		TrackMaterialInstance = VehicleMesh->CreateDynamicMaterialInstance(TrackMaterialSlotIndex);
	}

	// 相机初值
	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, CameraRelativeYaw, 0.0f));
	}
	DesiredGunPitch = FMath::Clamp(0.0f, MinPitch, MaxPitch);

	// 重生保护（仅服务器置位，客户端靠复制）
	if (HasAuthority() && SpawnProtectionDuration > 0.0f)
	{
		bSpawnProtected = true;
		GetWorldTimerManager().SetTimer(SpawnProtectionTimerHandle, this,
			&ATankVehicle::ClearSpawnProtection, SpawnProtectionDuration, false);
		OnRep_SpawnProtected();
	}

	LogPhysicsSetupState(TEXT("BeginPlay"));
	FTimerHandle DiagTimer;
	GetWorldTimerManager().SetTimer(DiagTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		LogPhysicsSetupState(TEXT("+1.2s"));
	}), 1.2f, false);

	AddDefaultMappingContext();
	EnsureClientReady();

	if (!HasAuthority())
	{
		GetWorldTimerManager().SetTimer(TickRepairTimerHandle, this, &ATankVehicle::RepairTickTimer, 1.0f, true);
	}
}

// ============================================================================
// 逐帧主循环 (Tick)
// ============================================================================
void ATankVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 0. 防悬空休眠
	if (VehicleMovement && VehicleMovement->PhysicsVehicleOutput().IsValid())
	{
		for (const UChaosVehicleWheel* Wheel : VehicleMovement->Wheels)
		{
			if (Wheel && Wheel->IsInAir())
			{
				VehicleMovement->SetSleeping(false);
				break;
			}
		}
	}

	// 1. 驱动：坦克差速力矩
	ApplyDriveInput();

	// 2. 视觉：负重轮、履带 UV、炮塔与瞄准
	UpdateWheelVisuals();
	UpdateTrackScroll(DeltaTime);
	UpdateTurretVisuals(DeltaTime);

	// 3. 调试输出
	if (bDrawDebugStatus)
	{
		DrawDebugStatus();
	}

	// 4. 每帧 Triggered 消费完清零
	CurrentThrottleInput = 0.0f;
	CurrentSteerInput = 0.0f;
	CurrentTurretRotateInput = 0.0f;
}

// ============================================================================
// 属性复制声明与辅助查询
// ============================================================================
void ATankVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATankVehicle, bSpawnProtected);

	// 炮塔/火炮朝向：给其他端看的状态（本机自算，COND_SkipOwner）
	DOREPLIFETIME_CONDITION(ATankVehicle, NetTurretYaw, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ATankVehicle, NetGunPitch, COND_SkipOwner);
}

float ATankVehicle::GetBodyHalfHeight() const
{
	return (ChassisBoxCenterZ + ChassisBoxHalfZ) * FMath::Max(VehicleScale, 0.01f);
}

#include "DefWall.h"
#include "WallHealth.h"
#include "ClimbManager.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ADefWall::ADefWall()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 墙体：2000×200×774cm，引擎基础方块缩放（BlockAll——丧尸扫掠被它挡住、炮弹打它爆炸）
	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	RootComponent = WallMesh;
	WallMesh->SetRelativeScale3D(FVector(20.0f, 2.0f, 7.74f));
	WallMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		WallMesh->SetStaticMesh(CubeMeshAsset.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterialAsset.Succeeded())
	{
		WallMesh->SetMaterial(0, ShapeMaterialAsset.Object);
	}

	// 2. 血量组件（纯逻辑，无需挂接）
	WallHealth = CreateDefaultSubobject<UWallHealthComponent>(TEXT("WallHealth"));

	// 3. 聚集闸门：聚集数达标 → 开闸攀爬 → 登顶经此扣墙血
	ClimbManager = CreateDefaultSubobject<UClimbManager>(TEXT("ClimbManager"));
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "TopDownProjPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "TopDownProjCharacter.h"
#include "Camera/CameraComponent.h"
#include "Actions/PawnActionsComponent.h"
#include "Actions/PawnAction_Move.h"
#include "Engine/World.h"

bool UPawnAction_Falling::Start()
{
	bool result = Super::Start();

	ACharacter* character = Cast<ACharacter>(GetPawn());

	if (result && character) character->LandedDelegate.AddDynamic(this, &UPawnAction_Falling::OnLanded);

	return result;
}

EPawnActionAbortState::Type UPawnAction_Falling::PerformAbort(EAIForceParam::Type ShouldForce)
{
	if (ACharacter* character = Cast<ACharacter>(GetPawn())) character->LandedDelegate.RemoveDynamic(this, &UPawnAction_Falling::OnLanded);
	return Super::PerformAbort(ShouldForce);
}

void UPawnAction_Falling::OnLanded(const FHitResult& Hit)
{	
	if (ACharacter* character = Cast<ACharacter>(GetPawn())) character->LandedDelegate.RemoveDynamic(this, &UPawnAction_Falling::OnLanded);
	Finish(EPawnActionResult::Success);
}



UPawnAction_PrimaryMove::UPawnAction_PrimaryMove(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWantsTick = 1;
	CameraComp = nullptr;
}

bool UPawnAction_PrimaryMove::Start()
{
	bool result = Super::Start();

	if (result)
	{
		ATopDownProjPlayerController::SharedData.bHasActivePrimaryMove = 1;
		
		if (APawn* pawn = GetPawn())
		{
			CameraComp = pawn->FindComponentByClass<UCameraComponent>();
		}
	}

	return result;
}

EPawnActionAbortState::Type UPawnAction_PrimaryMove::PerformAbort(EAIForceParam::Type ShouldForce)
{
	ATopDownProjPlayerController::SharedData.bHasActivePrimaryMove = 0;
	return Super::PerformAbort(ShouldForce);
}

void UPawnAction_PrimaryMove::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ATopDownProjPlayerController::SharedData.MoveX != 0 || ATopDownProjPlayerController::SharedData.MoveY != 0)
	{
		FRotator YawRotation(0, 0, 0);
		
		if (CameraComp)
		{
			YawRotation.Yaw = CameraComp->GetComponentRotation().Yaw;
		}

		FRotationMatrix rotmatrix(YawRotation);
		const FVector xDir = rotmatrix.GetUnitAxis(EAxis::X);
		const FVector yDir = rotmatrix.GetUnitAxis(EAxis::Y);
		GetPawn()->AddMovementInput(xDir * ATopDownProjPlayerController::SharedData.MoveY + yDir * ATopDownProjPlayerController::SharedData.MoveX);
	}
	else
	{
		ATopDownProjPlayerController::SharedData.bHasActivePrimaryMove = 0;
		Finish(EPawnActionResult::Success);
	}
}



UPawnAction_TargetMove::UPawnAction_TargetMove(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWantsTick = 1;
	PlayerController = nullptr;
}

bool UPawnAction_TargetMove::Start()
{
	bool result = Super::Start();

	if (result)
	{
		ATopDownProjPlayerController::SharedData.bHasActiveTargetMove = 1;

		if (APawn* pawn = GetPawn())
		{
			PlayerController = pawn->GetController<APlayerController>();
		}
	}

	return result;
}

EPawnActionAbortState::Type UPawnAction_TargetMove::PerformAbort(EAIForceParam::Type ShouldForce)
{
	ATopDownProjPlayerController::SharedData.bHasActiveTargetMove = 0;
	return Super::PerformAbort(ShouldForce);
}

void UPawnAction_TargetMove::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ATopDownProjPlayerController::SharedData.bInputPressed)
	{
		if (APawn* const MyPawn = GetPawn())
		{
			FHitResult Hit;

			bool hasHitResult = false;

			if (PlayerController)
			{
				hasHitResult = ATopDownProjPlayerController::SharedData.bIsTouch
					? PlayerController->GetHitResultUnderFinger(ETouchIndex::Touch1, ECC_Visibility, true, Hit)
					: PlayerController->GetHitResultUnderCursor(ECC_Visibility, true, Hit);
			}

			if (hasHitResult)
			{
				FVector WorldDirection = (Hit.Location - MyPawn->GetActorLocation()).GetSafeNormal();
				MyPawn->AddMovementInput(WorldDirection, 1.f, false);
			}
		}
	}
	else
	{
		ATopDownProjPlayerController::SharedData.bHasActiveTargetMove = 0;
		Finish(EPawnActionResult::Success);
	}
}



decltype(ATopDownProjPlayerController::SharedData) ATopDownProjPlayerController::SharedData;

void StartAction(UPawnActionsComponent* actionsComp, TFunctionRef<UPawnAction* (UWorld*)> actionCreator, EAIRequestPriority::Type aiRequestPriority, bool abortCurrentAction)
{
	if (actionsComp)
	{
		if (UPawnAction* currentAction = actionsComp->GetCurrentAction())
		{
			EAIRequestPriority::Type currentAIPriority = currentAction->GetPriority();

			if (currentAIPriority > aiRequestPriority) return; // Is busy with more priority

			if (abortCurrentAction)
			{
				bool isForced = aiRequestPriority == EAIRequestPriority::Reaction;

				EPawnActionAbortState::Type abortState = isForced ? actionsComp->ForceAbortAction(*currentAction) : actionsComp->AbortAction(*currentAction);
			}
		}

		actionsComp->PushAction(*actionCreator(actionsComp->GetWorld()), aiRequestPriority);
	}
}

void OnStartPrimaryMove(UPawnActionsComponent* actionsComp)
{
	if (actionsComp)
	{
		StartAction(actionsComp, [](UWorld* world) { return UPawnAction_PrimaryMove::CreateAction(*world); }, EAIRequestPriority::Logic, true);
	}
}

void OnStartTargetMove(UPawnActionsComponent* actionsComp)
{
	if (actionsComp)
	{
		if (ATopDownProjPlayerController::SharedData.bHasActivePrimaryMove) return; // Primary Move is top priority among movements

		StartAction(actionsComp, [](UWorld* world) {return UPawnAction_TargetMove::CreateAction(*world); }, EAIRequestPriority::Logic, true);
	}
}

void OnStartFalling(UPawnActionsComponent* actionsComp, bool isJump)
{
	if (actionsComp)
	{
		StartAction(actionsComp, [isJump](UWorld* world) { return UPawnAction_Falling::CreateAction(*world); }, EAIRequestPriority::Reaction, !isJump);
	}
}



ATopDownProjPlayerController::ATopDownProjPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;

	ActionsComp = CreateDefaultSubobject<UPawnActionsComponent>("ActionsComp");

	SharedData.MoveX = 0;
	SharedData.MoveY = 0;

	SharedData.bIsTouch = 0;

	SharedData.bInputPressed = 0;

	SharedData.bHasActivePrimaryMove = 0;

	SharedData.bHasActiveTargetMove = 0;
}

void ATopDownProjPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (SharedData.MoveX != 0 || SharedData.MoveY != 0)
	{
		if (!SharedData.bHasActivePrimaryMove)
		{
			OnStartPrimaryMove(ActionsComp);
		}
	}
	else if (SharedData.bInputPressed)
	{
		if (!SharedData.bHasActiveTargetMove)
		{
			OnStartTargetMove(ActionsComp);
		}
	}
}

void ATopDownProjPlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	InputComponent->BindAction("SetDestination", IE_Pressed, this, &ATopDownProjPlayerController::OnSetDestinationPressed);
	InputComponent->BindAction("SetDestination", IE_Released, this, &ATopDownProjPlayerController::OnSetDestinationReleased);

	// support touch devices 
	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &ATopDownProjPlayerController::OnTouchPressed);
	InputComponent->BindTouch(EInputEvent::IE_Released, this, &ATopDownProjPlayerController::OnTouchReleased);

	InputComponent->BindAction("Jump", IE_Pressed, this, &ATopDownProjPlayerController::OnJump);

	// support manual movement
	InputComponent->BindAxis("Move Right/Left", this, &ATopDownProjPlayerController::OnMoveX);
	InputComponent->BindAxis("Move Forward/Backward", this, &ATopDownProjPlayerController::OnMoveY);

}

void ATopDownProjPlayerController::OnJump() { if (ACharacter* character = GetPawn<ACharacter>()) character->Jump(); }



void ATopDownProjCharacter::Falling()
{
	if (ATopDownProjPlayerController* topDownProjPlayerController = Cast<ATopDownProjPlayerController>(GetController()))
	{
		OnStartFalling(topDownProjPlayerController->GetActionsComp(), bPressedJump);
	}

	Super::Falling();
}
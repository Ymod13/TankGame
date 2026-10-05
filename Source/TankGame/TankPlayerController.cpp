// Created by Domenico Guaccero - 2026


#include "TankPlayerController.h"
#include "Blueprint/UserWidget.h"  
#include "Blueprint/WidgetBlueprintLibrary.h"   

void ATankPlayerController::BeginPlay()
{
    if (ScoreHUDClass)
    {
        if (UUserWidget* HUDWidget = CreateWidget<UUserWidget>(this, ScoreHUDClass))
        {
            HUDWidget->AddToViewport();
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
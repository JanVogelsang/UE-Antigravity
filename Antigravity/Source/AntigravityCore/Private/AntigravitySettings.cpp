// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravitySettings.h"
#include "AntigravityCoreModule.h"
#include "Misc/MessageDialog.h"

FAntigravitySettingsChangedDelegate UAntigravityDeveloperSettings::OnSettingsChanged;

UAntigravityDeveloperSettings::UAntigravityDeveloperSettings()
{
	// Safety defaults -- Full Access by default
	SecurityMode = EAntigravitySecurityMode::FullAccess;
}

const UAntigravityDeveloperSettings* UAntigravityDeveloperSettings::Get()
{
	return GetDefault<UAntigravityDeveloperSettings>();
}

bool UAntigravityDeveloperSettings::IsIniSectionAllowed(const FString& Section) const
{
	if (SecurityMode == EAntigravitySecurityMode::Restricted || SecurityMode == EAntigravitySecurityMode::Standard)
	{
		return Section.StartsWith(TEXT("/Script/Antigravity"));
	}
	return true;
}

FName UAntigravityDeveloperSettings::GetContainerName() const
{
	return TEXT("Project");
}

FName UAntigravityDeveloperSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UAntigravityDeveloperSettings::GetSectionName() const
{
	return TEXT("Antigravity");
}

#if WITH_EDITOR
FText UAntigravityDeveloperSettings::GetSectionText() const
{
	return FText::FromString(TEXT("Antigravity AI Assistant"));
}

FText UAntigravityDeveloperSettings::GetSectionDescription() const
{
	return FText::FromString(TEXT("Configure the Antigravity AI assistant plugin -- safety settings and protected files."));
}

void UAntigravityDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property) return;

	FName PropName = PropertyChangedEvent.Property->GetFName();

	// ====================================================================
	// CRITICAL: Developer mode escalation confirmation dialog
	// Switching to Developer mode requires explicit user confirmation.
	// ====================================================================
	if (PropName == GET_MEMBER_NAME_CHECKED(UAntigravityDeveloperSettings, SecurityMode))
	{
		if (SecurityMode == EAntigravitySecurityMode::FullAccess)
		{
			FText Title = FText::FromString(TEXT("Enable Full Access Mode?"));
			FText Message = FText::FromString(
				TEXT("WARNING: Full Access Mode allows:\n\n")
				TEXT("- C++ file writes and compilation\n")
				TEXT("- Full INI/config modification\n")
				TEXT("- External process execution (UAT builds)\n")
				TEXT("- Source control operations\n")
				TEXT("- Project-wide mutation\n\n")
				TEXT("This gives the AI full power over your project.\n")
				TEXT("Only enable if you understand the risks.\n\n")
				TEXT("Continue?")
			);

			EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Message, Title);

			if (Result != EAppReturnType::Yes)
			{
				// User declined -- revert to Standard
				SecurityMode = EAntigravitySecurityMode::Standard;
				UE_LOG(LogAntigravity, Log, TEXT("Antigravity: Full Access mode switch declined. Reverting to Standard."));
			}
		}
	}

	OnSettingsChanged.Broadcast(PropName);
}
#endif

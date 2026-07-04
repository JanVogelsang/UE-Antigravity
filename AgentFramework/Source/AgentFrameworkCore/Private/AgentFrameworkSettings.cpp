// Copyright 2026 AgentFramework. All Rights Reserved.

#include "AgentFrameworkSettings.h"
#include "AgentFrameworkCoreModule.h"
#include "Misc/MessageDialog.h"

FAgentFrameworkSettingsChangedDelegate UAgentFrameworkDeveloperSettings::OnSettingsChanged;

UAgentFrameworkDeveloperSettings::UAgentFrameworkDeveloperSettings()
{
	// Safety defaults -- Full Access by default
	SecurityMode = EAgentFrameworkSecurityMode::FullAccess;
}

const UAgentFrameworkDeveloperSettings* UAgentFrameworkDeveloperSettings::Get()
{
	return GetDefault<UAgentFrameworkDeveloperSettings>();
}

bool UAgentFrameworkDeveloperSettings::IsIniSectionAllowed(const FString& Section) const
{
	if (SecurityMode == EAgentFrameworkSecurityMode::Restricted || SecurityMode == EAgentFrameworkSecurityMode::Standard)
	{
		return Section.StartsWith(TEXT("/Script/AgentFramework"));
	}
	return true;
}

FName UAgentFrameworkDeveloperSettings::GetContainerName() const
{
	return TEXT("Project");
}

FName UAgentFrameworkDeveloperSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UAgentFrameworkDeveloperSettings::GetSectionName() const
{
	return TEXT("AgentFramework");
}

#if WITH_EDITOR
FText UAgentFrameworkDeveloperSettings::GetSectionText() const
{
	return FText::FromString(TEXT("AgentFramework AI Assistant"));
}

FText UAgentFrameworkDeveloperSettings::GetSectionDescription() const
{
	return FText::FromString(TEXT("Configure the AgentFramework AI assistant plugin -- safety settings and protected files."));
}

void UAgentFrameworkDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property) return;

	FName PropName = PropertyChangedEvent.Property->GetFName();

	// ====================================================================
	// CRITICAL: Developer mode escalation confirmation dialog
	// Switching to Developer mode requires explicit user confirmation.
	// ====================================================================
	if (PropName == GET_MEMBER_NAME_CHECKED(UAgentFrameworkDeveloperSettings, SecurityMode))
	{
		if (SecurityMode == EAgentFrameworkSecurityMode::FullAccess)
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
				SecurityMode = EAgentFrameworkSecurityMode::Standard;
				UE_LOG(LogAgentFramework, Log, TEXT("AgentFramework: Full Access mode switch declined. Reverting to Standard."));
			}
		}
	}

	OnSettingsChanged.Broadcast(PropName);
}
#endif

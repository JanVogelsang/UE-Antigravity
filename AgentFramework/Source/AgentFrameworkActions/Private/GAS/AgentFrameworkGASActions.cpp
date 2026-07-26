// Copyright 2026 AgentFramework. All Rights Reserved.

#include "GAS/AgentFrameworkGASActions.h"
#include "AgentFrameworkActionUtils.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsEditorModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Sound/SoundBase.h"
#endif

#define LOCTEXT_NAMESPACE "AgentFrameworkGASActions"

// ============================================================================
// Helper: Set a protected UPROPERTY on a CDO via reflection
// ============================================================================

namespace AgentFrameworkGASReflection
{
	/** Set an enum property by name on an object via reflection */
	template<typename EnumType>
	bool SetEnumProperty(UObject* Object, const FString& PropertyName, EnumType Value)
	{
		if (!IsValid(Object)) return false;
		FProperty* Prop = Object->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Prop) return false;

		FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop);
		FByteProperty* ByteProp = CastField<FByteProperty>(Prop);

		if (EnumProp)
		{
			void* ValuePtr = EnumProp->ContainerPtrToValuePtr<void>(Object);
			if (ValuePtr && EnumProp->GetUnderlyingProperty())
			{
				EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, static_cast<int64>(Value));
				return true;
			}
		}
		else if (ByteProp)
		{
			ByteProp->SetPropertyValue_InContainer(Object, static_cast<uint8>(Value));
			return true;
		}
		return false;
	}

	/** Set a TSubclassOf<> property by name on an object via reflection */
	bool SetClassProperty(UObject* Object, const FString& PropertyName, UClass* ClassValue)
	{
		if (!IsValid(Object) || !IsValid(ClassValue)) return false;
		FClassProperty* Prop = CastField<FClassProperty>(Object->GetClass()->FindPropertyByName(FName(*PropertyName)));
		if (!Prop) return false;
		Prop->SetObjectPropertyValue_InContainer(Object, ClassValue);
		return true;
	}

	/** Add a tag to a FGameplayTagContainer property on an object via reflection */
	bool AddTagToContainer(UObject* Object, const FString& PropertyName, FGameplayTag Tag)
	{
		if (!IsValid(Object) || !Tag.IsValid()) return false;
		FProperty* Prop = Object->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Prop) return false;
		FStructProperty* StructProp = CastField<FStructProperty>(Prop);
		if (!StructProp) return false;
		FGameplayTagContainer* Container = StructProp->ContainerPtrToValuePtr<FGameplayTagContainer>(Object);
		if (!Container) return false;
		Container->AddTag(Tag);
		return true;
	}
}

// ============================================================================
// Lifecycle
// ============================================================================

FAgentFrameworkGASActions::FAgentFrameworkGASActions() {}
FAgentFrameworkGASActions::~FAgentFrameworkGASActions() {}

void FAgentFrameworkGASActions::PlaySuccessSound()
{
#if WITH_EDITOR
	if (IsValid(GEditor))
	{
		USoundBase* SuccessSound = LoadObject<USoundBase>(nullptr, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"));
		if (IsValid(SuccessSound))
		{
			GEditor->PlayEditorSound(SuccessSound);
		}
	}
#endif
}

// ============================================================================
// IAgentFrameworkActionExecutor Interface
// ============================================================================

FName FAgentFrameworkGASActions::GetActionName() const { return FName(TEXT("GAS")); }

TArray<FString> FAgentFrameworkGASActions::GetSupportedToolNames() const
{
	return {
		TEXT("gas_register_tags"),
		TEXT("gas_create_attribute_set"),
		TEXT("gas_setup_asc"),
		TEXT("gas_create_effect"),
		TEXT("gas_create_ability")
	};
}

bool FAgentFrameworkGASActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkGASActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString Action;
	TArray<FString> TempErrors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), Action, TempErrors, false) || Action.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), Action, TempErrors, false);
	}

	if (Action == TEXT("gas_register_tags"))
		Result = ExecuteRegisterTags(Params, Result);
	else if (Action == TEXT("gas_create_attribute_set"))
		Result = ExecuteCreateAttributeSet(Params, Result);
	else if (Action == TEXT("gas_setup_asc"))
		Result = ExecuteSetupASC(Params, Result);
	else if (Action == TEXT("gas_create_effect"))
		Result = ExecuteCreateEffect(Params, Result);
	else if (Action == TEXT("gas_create_ability"))
		Result = ExecuteCreateAbility(Params, Result);
	else
		Result.Errors.Add(TEXT("Unknown GAS action. Use gas_register_tags, gas_create_attribute_set, gas_setup_asc, gas_create_effect, or gas_create_ability."));

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}

	return Result;
}

// ============================================================================
// gas_register_tags
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkGASActions::ExecuteRegisterTags(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
	if (!UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("tags"), TagsArray, Result.Errors, true) || !TagsArray || TagsArray->Num() == 0)
	{
		if (Result.Errors.Num() == 0)
		{
			Result.Errors.Add(TEXT("Missing required field: 'tags' — array of {tag, comment} objects"));
		}
		return Result;
	}

	IGameplayTagsEditorModule& TagsEditor = IGameplayTagsEditorModule::Get();
	int32 TagsAdded = 0;
	FString Report;

	for (const TSharedPtr<FJsonValue>& TagVal : *TagsArray)
	{
		if (!TagVal.IsValid()) continue;
		const TSharedPtr<FJsonObject>* TagObjPtr = nullptr;
		if (!TagVal->TryGetObject(TagObjPtr) || !TagObjPtr || !TagObjPtr->IsValid()) continue;
		const TSharedPtr<FJsonObject>& TagObj = *TagObjPtr;

		FString TagName, Comment;
		TArray<FString> IgnoreErrors;
		UAgentFrameworkActionUtils::TryGetStringParam(TagObj, TEXT("tag"), TagName, IgnoreErrors, false);
		UAgentFrameworkActionUtils::TryGetStringParam(TagObj, TEXT("comment"), Comment, IgnoreErrors, false);

		if (TagName.IsEmpty()) continue;

		FGameplayTag ExistingTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName), false);
		if (ExistingTag.IsValid())
		{
			Report += FString::Printf(TEXT("  [SKIP] '%s' (already exists)\n"), *TagName);
			continue;
		}

		TagsEditor.AddNewGameplayTagToINI(TagName, Comment.IsEmpty() ? TEXT("Added by AgentFramework") : Comment);
		Report += FString::Printf(TEXT("  [ADD] '%s' — %s\n"), *TagName, *Comment);
		TagsAdded++;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Registered %d gameplay tag(s):\n%s"), TagsAdded, *Report);
	return Result;
}

// ============================================================================
// gas_create_attribute_set
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkGASActions::ExecuteCreateAttributeSet(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString ClassName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("class_name"), ClassName, Result.Errors, true))
	{
		return Result;
	}

	if (!ClassName.StartsWith(TEXT("U")))
		ClassName = TEXT("U") + ClassName;

	FString FileName = ClassName.Mid(1);

	const TArray<TSharedPtr<FJsonValue>>* AttributesArray = nullptr;
	if (!UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("attributes"), AttributesArray, Result.Errors, true) || !AttributesArray || AttributesArray->Num() == 0)
	{
		if (Result.Errors.Num() == 0)
		{
			Result.Errors.Add(TEXT("Missing required field: 'attributes'"));
		}
		return Result;
	}

	TArray<FString> IgnoreErrors;
	FString ModuleName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("module_name"), ModuleName, IgnoreErrors, false) || ModuleName.IsEmpty())
	{
		FString ProjectName = FString(FApp::GetProjectName());
		ModuleName = ProjectName.ToUpper() + TEXT("_API");
	}

	// Build header
	FString Header;
	Header += TEXT("// Generated by AgentFramework — Gameplay Ability System AttributeSet\n\n");
	Header += TEXT("#pragma once\n\n");
	Header += TEXT("#include \"CoreMinimal.h\"\n");
	Header += TEXT("#include \"AttributeSet.h\"\n");
	Header += TEXT("#include \"AbilitySystemComponent.h\"\n");
	Header += TEXT("#include \"Net/UnrealNetwork.h\"\n");
	Header += FString::Printf(TEXT("#include \"%s.generated.h\"\n\n"), *FileName);

	Header += TEXT("#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \\\n");
	Header += TEXT("\tGAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \\\n");
	Header += TEXT("\tGAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \\\n");
	Header += TEXT("\tGAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \\\n");
	Header += TEXT("\tGAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)\n\n");

	Header += TEXT("UCLASS()\n");
	Header += FString::Printf(TEXT("class %s %s : public UAttributeSet\n"), *ModuleName, *ClassName);
	Header += TEXT("{\n\tGENERATED_BODY()\n\npublic:\n");
	Header += FString::Printf(TEXT("\t%s();\n\n"), *ClassName);
	Header += TEXT("\tvirtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;\n\n");

	TArray<FString> AttributeNames;
	for (const TSharedPtr<FJsonValue>& AttrVal : *AttributesArray)
	{
		if (!AttrVal.IsValid()) continue;
		const TSharedPtr<FJsonObject>* AttrObjPtr = nullptr;
		if (!AttrVal->TryGetObject(AttrObjPtr) || !AttrObjPtr || !AttrObjPtr->IsValid()) continue;

		FString AttrName;
		UAgentFrameworkActionUtils::TryGetStringParam(*AttrObjPtr, TEXT("name"), AttrName, IgnoreErrors, false);
		if (AttrName.IsEmpty()) continue;

		bool bReplicated = true;
		UAgentFrameworkActionUtils::TryGetBoolParam(*AttrObjPtr, TEXT("replicated"), bReplicated, IgnoreErrors, false);

		AttributeNames.Add(AttrName);

		if (bReplicated)
			Header += FString::Printf(TEXT("\tUPROPERTY(BlueprintReadOnly, Category = \"Attributes\", ReplicatedUsing = OnRep_%s)\n"), *AttrName);
		else
			Header += TEXT("\tUPROPERTY(BlueprintReadOnly, Category = \"Attributes\")\n");

		Header += FString::Printf(TEXT("\tFGameplayAttributeData %s;\n"), *AttrName);
		Header += FString::Printf(TEXT("\tATTRIBUTE_ACCESSORS(%s, %s)\n\n"), *ClassName, *AttrName);

		if (bReplicated)
			Header += FString::Printf(TEXT("\tUFUNCTION()\n\tvirtual void OnRep_%s(const FGameplayAttributeData& Old%s);\n\n"), *AttrName, *AttrName);
	}
	Header += TEXT("};\n");

	// Build source
	FString Source;
	Source += FString::Printf(TEXT("#include \"%s.h\"\n#include \"Net/UnrealNetwork.h\"\n\n"), *FileName);
	Source += FString::Printf(TEXT("%s::%s() {\n"), *ClassName, *ClassName);
	for (const FString& A : AttributeNames)
		Source += FString::Printf(TEXT("\tInit%s(100.0f);\n"), *A);
	Source += TEXT("}\n\n");

	Source += FString::Printf(TEXT("void %s::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {\n"), *ClassName);
	Source += TEXT("\tSuper::GetLifetimeReplicatedProps(OutLifetimeProps);\n");
	for (const FString& A : AttributeNames)
		Source += FString::Printf(TEXT("\tDOREPLIFETIME_CONDITION_NOTIFY(%s, %s, COND_None, REPNOTIFY_Always);\n"), *ClassName, *A);
	Source += TEXT("}\n\n");

	for (const FString& A : AttributeNames)
	{
		Source += FString::Printf(TEXT("void %s::OnRep_%s(const FGameplayAttributeData& Old%s) {\n"), *ClassName, *A, *A);
		Source += FString::Printf(TEXT("\tGAMEPLAYATTRIBUTE_REPNOTIFY(%s, %s, Old%s);\n}\n\n"), *ClassName, *A, *A);
	}

	// Write files
	FString SourceDir = FPaths::GameSourceDir();
	FString SubDir;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("source_directory"), SubDir, IgnoreErrors, false);
	if (SubDir.IsEmpty()) SubDir = FString(FApp::GetProjectName());

	FString HeaderPath = FPaths::Combine(SourceDir, SubDir, FileName + TEXT(".h"));
	FString SourcePath = FPaths::Combine(SourceDir, SubDir, FileName + TEXT(".cpp"));

	FFileHelper::SaveStringToFile(Header, *HeaderPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FFileHelper::SaveStringToFile(Source, *SourcePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	Result.bSuccess = true;
	Result.ModifiedPaths.Add(HeaderPath);
	Result.ModifiedPaths.Add(SourcePath);
	Result.ResultMessage = FString::Printf(
		TEXT("Created AttributeSet '%s' with %d attributes.\n  Header: %s\n  Source: %s\n\n"
			 "IMPORTANT: Run trigger_compile before using other GAS tools."), *ClassName, AttributeNames.Num(), *HeaderPath, *SourcePath);
	return Result;
}

// ============================================================================
// gas_setup_asc
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkGASActions::ExecuteSetupASC(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!IsValid(SCS))
	{
		Result.Errors.Add(TEXT("Blueprint has no SimpleConstructionScript."));
		return Result;
	}

	// Check if ASC already exists
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (IsValid(Node) && IsValid(Node->ComponentClass) && Node->ComponentClass->IsChildOf(UAbilitySystemComponent::StaticClass()))
		{
			Result.bSuccess = true;
			Result.ResultMessage = FString::Printf(TEXT("ASC already exists on '%s'. No changes."), *AssetPath);
			return Result;
		}
	}

	Blueprint->Modify();
	USCS_Node* ASCNode = SCS->CreateNode(UAbilitySystemComponent::StaticClass(), TEXT("AbilitySystemComp"));
	if (!IsValid(ASCNode))
	{
		Result.Errors.Add(TEXT("Failed to create ASC SCS node."));
		return Result;
	}
	SCS->AddNode(ASCNode);

	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	Blueprint->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(
		TEXT("Added AbilitySystemComponent to '%s'.\n\n"
			 "NEXT: Implement IAbilitySystemInterface in C++ and call InitAbilityActorInfo."), *AssetPath);
	return Result;
}

// ============================================================================
// gas_create_effect
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkGASActions::ExecuteCreateEffect(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	if (!IsValid(Factory))
	{
		Result.Errors.Add(TEXT("Failed to create UBlueprintFactory."));
		return Result;
	}
	Factory->ParentClass = UGameplayEffect::StaticClass();
	Factory->bSkipClassPicker = true;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UBlueprint::StaticClass(), Factory);
	if (!IsValid(NewAsset))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create GE at '%s'."), *AssetPath));
		return Result;
	}

	UBlueprint* GEBlueprint = Cast<UBlueprint>(NewAsset);
	if (!IsValid(GEBlueprint) || !IsValid(GEBlueprint->GeneratedClass))
	{
		Result.Errors.Add(TEXT("Invalid GE Blueprint."));
		return Result;
	}

	UGameplayEffect* GECDO = Cast<UGameplayEffect>(GEBlueprint->GeneratedClass->GetDefaultObject());
	if (!IsValid(GECDO))
	{
		Result.Errors.Add(TEXT("No GE CDO."));
		return Result;
	}

	GECDO->Modify();
	FString Report;
	TArray<FString> IgnoreErrors;

	// Duration Policy
	FString DurationStr;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("duration_policy"), DurationStr, IgnoreErrors, false);
	DurationStr = DurationStr.ToLower();

	if (DurationStr == TEXT("instant"))
	{
		GECDO->DurationPolicy = EGameplayEffectDurationType::Instant;
		Report += TEXT("Duration: Instant\n");
	}
	else if (DurationStr == TEXT("has_duration") || DurationStr == TEXT("duration"))
	{
		GECDO->DurationPolicy = EGameplayEffectDurationType::HasDuration;
		float Duration = 5.0f;
		UAgentFrameworkActionUtils::TryGetFloatParam(Params, TEXT("duration_seconds"), Duration, IgnoreErrors, false);
		GECDO->DurationMagnitude = FScalableFloat(Duration);
		Report += FString::Printf(TEXT("Duration: %.1fs\n"), Duration);
	}
	else if (DurationStr == TEXT("infinite"))
	{
		GECDO->DurationPolicy = EGameplayEffectDurationType::Infinite;
		Report += TEXT("Duration: Infinite\n");
	}
	else
	{
		GECDO->DurationPolicy = EGameplayEffectDurationType::Instant;
		Report += TEXT("Duration: Instant (default)\n");
	}

	// Modifiers
	const TArray<TSharedPtr<FJsonValue>>* ModifiersArray = nullptr;
	if (UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("modifiers"), ModifiersArray, IgnoreErrors, false) && ModifiersArray)
	{
		for (const TSharedPtr<FJsonValue>& ModVal : *ModifiersArray)
		{
			if (!ModVal.IsValid()) continue;
			const TSharedPtr<FJsonObject>* ModObjPtr = nullptr;
			if (!ModVal->TryGetObject(ModObjPtr) || !ModObjPtr || !ModObjPtr->IsValid()) continue;

			FGameplayModifierInfo Modifier;
			FString OpStr;
			UAgentFrameworkActionUtils::TryGetStringParam(*ModObjPtr, TEXT("operation"), OpStr, IgnoreErrors, false);
			OpStr = OpStr.ToLower();

			if (OpStr == TEXT("add") || OpStr == TEXT("additive"))
				Modifier.ModifierOp = EGameplayModOp::Additive;
			else if (OpStr == TEXT("multiply"))
				Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
			else if (OpStr == TEXT("divide"))
				Modifier.ModifierOp = EGameplayModOp::Division;
			else if (OpStr == TEXT("override"))
				Modifier.ModifierOp = EGameplayModOp::Override;
			else
				Modifier.ModifierOp = EGameplayModOp::Additive;

			float Magnitude = 0.0f;
			UAgentFrameworkActionUtils::TryGetFloatParam(*ModObjPtr, TEXT("magnitude"), Magnitude, IgnoreErrors, false);
			Modifier.ModifierMagnitude = FScalableFloat(Magnitude);

			FString AttributeStr;
			UAgentFrameworkActionUtils::TryGetStringParam(*ModObjPtr, TEXT("attribute"), AttributeStr, IgnoreErrors, false);

			GECDO->Modifiers.Add(Modifier);
			Report += FString::Printf(TEXT("Modifier: %s %.1f to %s\n"), *OpStr, Magnitude, *AttributeStr);
		}
	}

	// Grant Tags
	TArray<FString> GrantTagStrings;
	if (UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("grant_tags"), GrantTagStrings, IgnoreErrors, false))
	{
		for (const FString& TagStr : GrantTagStrings)
		{
			if (TagStr.IsEmpty()) continue;
			FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
			if (Tag.IsValid())
			{
				AgentFrameworkGASReflection::AddTagToContainer(GECDO, TEXT("InheritableOwnedTagsContainer"), Tag);
				Report += FString::Printf(TEXT("Owned tag: %s\n"), *TagStr);
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("Tag '%s' not found. Register first."), *TagStr));
			}
		}
	}

	// Asset Tags
	TArray<FString> AssetTagStrings;
	if (UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("asset_tags"), AssetTagStrings, IgnoreErrors, false))
	{
		for (const FString& TagStr : AssetTagStrings)
		{
			if (TagStr.IsEmpty()) continue;
			FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
			if (Tag.IsValid())
			{
				AgentFrameworkGASReflection::AddTagToContainer(GECDO, TEXT("InheritableGameplayEffectTags"), Tag);
				Report += FString::Printf(TEXT("Asset tag: %s\n"), *TagStr);
			}
		}
	}

	FKismetEditorUtilities::CompileBlueprint(GEBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	GEBlueprint->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Created GameplayEffect '%s':\n%s"), *AssetPath, *Report);
	return Result;
}

// ============================================================================
// gas_create_ability
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkGASActions::ExecuteCreateAbility(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	if (!IsValid(Factory))
	{
		Result.Errors.Add(TEXT("Failed to create UBlueprintFactory."));
		return Result;
	}
	Factory->ParentClass = UGameplayAbility::StaticClass();
	Factory->bSkipClassPicker = true;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UBlueprint::StaticClass(), Factory);
	if (!IsValid(NewAsset))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create GA at '%s'."), *AssetPath));
		return Result;
	}

	UBlueprint* GABlueprint = Cast<UBlueprint>(NewAsset);
	if (!IsValid(GABlueprint) || !IsValid(GABlueprint->GeneratedClass))
	{
		Result.Errors.Add(TEXT("Invalid GA Blueprint."));
		return Result;
	}

	UGameplayAbility* GACDO = Cast<UGameplayAbility>(GABlueprint->GeneratedClass->GetDefaultObject());
	if (!IsValid(GACDO))
	{
		Result.Errors.Add(TEXT("No GA CDO."));
		return Result;
	}

	GACDO->Modify();
	FString Report;
	TArray<FString> IgnoreErrors;

	// Instancing Policy
	FString InstancingStr;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("instancing_policy"), InstancingStr, IgnoreErrors, false);
	InstancingStr = InstancingStr.ToLower();

	EGameplayAbilityInstancingPolicy::Type InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	if (InstancingStr == TEXT("instanced_per_execution"))
		InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	else if (InstancingStr == TEXT("non_instanced"))
		InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor; // NonInstanced is deprecated

	AgentFrameworkGASReflection::SetEnumProperty(GACDO, TEXT("InstancingPolicy"), InstancingPolicy);
	Report += FString::Printf(TEXT("Instancing: %s\n"), *InstancingStr);

	// Net Execution Policy
	FString NetPolicyStr;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("net_execution_policy"), NetPolicyStr, IgnoreErrors, false);
	NetPolicyStr = NetPolicyStr.ToLower();

	EGameplayAbilityNetExecutionPolicy::Type NetPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	if (NetPolicyStr == TEXT("server_only"))
		NetPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	else if (NetPolicyStr == TEXT("local_only"))
		NetPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	else if (NetPolicyStr == TEXT("server_initiated"))
		NetPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	AgentFrameworkGASReflection::SetEnumProperty(GACDO, TEXT("NetExecutionPolicy"), NetPolicy);
	Report += FString::Printf(TEXT("Net: %s\n"), *NetPolicyStr);

	// Ability Tags
	TArray<FString> AbilityTagStrings;
	if (UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("ability_tags"), AbilityTagStrings, IgnoreErrors, false))
	{
		for (const FString& TagStr : AbilityTagStrings)
		{
			if (TagStr.IsEmpty()) continue;
			FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
			if (Tag.IsValid())
			{
				AgentFrameworkGASReflection::AddTagToContainer(GACDO, TEXT("AbilityTags"), Tag);
				Report += FString::Printf(TEXT("Ability tag: %s\n"), *TagStr);
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("Tag '%s' not found."), *TagStr));
			}
		}
	}

	// Cooldown GE class
	FString CooldownGEPath;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("cooldown_effect"), CooldownGEPath, IgnoreErrors, false) && !CooldownGEPath.IsEmpty())
	{
		UBlueprint* CooldownBP = LoadObject<UBlueprint>(nullptr, *CooldownGEPath);
		if (IsValid(CooldownBP) && IsValid(CooldownBP->GeneratedClass))
		{
			AgentFrameworkGASReflection::SetClassProperty(GACDO, TEXT("CooldownGameplayEffectClass"), CooldownBP->GeneratedClass);
			Report += FString::Printf(TEXT("Cooldown GE: %s\n"), *CooldownGEPath);
		}
	}

	// Cost GE class
	FString CostGEPath;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("cost_effect"), CostGEPath, IgnoreErrors, false) && !CostGEPath.IsEmpty())
	{
		UBlueprint* CostBP = LoadObject<UBlueprint>(nullptr, *CostGEPath);
		if (IsValid(CostBP) && IsValid(CostBP->GeneratedClass))
		{
			AgentFrameworkGASReflection::SetClassProperty(GACDO, TEXT("CostGameplayEffectClass"), CostBP->GeneratedClass);
			Report += FString::Printf(TEXT("Cost GE: %s\n"), *CostGEPath);
		}
	}

	FKismetEditorUtilities::CompileBlueprint(GABlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	GABlueprint->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(
		TEXT("Created GameplayAbility '%s':\n%s\n"
			 "Use inject_blueprint_nodes_t3d to add the ability graph."), *AssetPath, *Report);
	return Result;
}

#undef LOCTEXT_NAMESPACE

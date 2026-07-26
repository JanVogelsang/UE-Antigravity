// Copyright 2026 AgentFramework. All Rights Reserved.

#include "MetaSound/AgentFrameworkMetaSoundActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ScopedTransaction.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// Engine & MetaSound Headers
#include "MetasoundSource.h"
#include "Interfaces/MetasoundOutputFormatInterfaces.h"
#include "MetasoundFactory.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocument.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Sound/SoundBase.h"
#endif

#define LOCTEXT_NAMESPACE "FAgentFrameworkMetaSoundActions"

FAgentFrameworkMetaSoundActions::FAgentFrameworkMetaSoundActions() {}
FAgentFrameworkMetaSoundActions::~FAgentFrameworkMetaSoundActions() {}

FName FAgentFrameworkMetaSoundActions::GetActionName() const
{
	return FName(TEXT("MetaSound"));
}

TArray<FString> FAgentFrameworkMetaSoundActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_metasound_source"),
		TEXT("wire_metasound_nodes")
	};
}

bool FAgentFrameworkMetaSoundActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, OutErrors, false);
	if (ToolName.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), ToolName, OutErrors, false);
	}
	if (ToolName.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), ToolName, OutErrors, false);
	}

	if (ToolName == TEXT("create_metasound_source") || ToolName.IsEmpty())
	{
		FString AssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, false) || AssetPath.IsEmpty())
		{
			FString DestPath, AssetName;
			if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("destination_path"), DestPath, OutErrors, false) ||
				!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_name"), AssetName, OutErrors, false))
			{
				OutErrors.Add(TEXT("Missing required parameter: 'asset_path' (or 'destination_path' and 'asset_name')."));
				return false;
			}
		}
	}
	else if (ToolName == TEXT("wire_metasound_nodes"))
	{
		FString AssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, true))
		{
			return false;
		}
	}

	return true;
}

FAgentFrameworkActionResult FAgentFrameworkMetaSoundActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, Result.Errors, false);
	if (ToolName.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), ToolName, Result.Errors, false);
	}
	if (ToolName.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), ToolName, Result.Errors, false);
	}

	if (ToolName == TEXT("create_metasound_source"))
	{
		Result = ExecuteCreateMetaSoundSource(Params, Result);
	}
	else if (ToolName == TEXT("wire_metasound_nodes"))
	{
		Result = ExecuteWireMetaSoundNodes(Params, Result);
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown or unspecified MetaSound action: '%s'"), *ToolName));
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}

	return Result;
}

static FMetasoundFrontendClassName ParseMetasoundClassName(const FString& InStr)
{
	FString MainStr = InStr;
	FString VariantStr;

	if (InStr.Split(TEXT(":"), &MainStr, &VariantStr))
	{
		// MainStr has namespace/name, VariantStr has variant
	}

	FString NamespaceStr = TEXT("UE");
	FString NameStr = MainStr;

	if (MainStr.Split(TEXT("."), &NamespaceStr, &NameStr))
	{
		// NamespaceStr and NameStr extracted
	}

	return FMetasoundFrontendClassName(*NamespaceStr, *NameStr, *VariantStr);
}

FAgentFrameworkActionResult FAgentFrameworkMetaSoundActions::ExecuteCreateMetaSoundSource(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, false) || AssetPath.IsEmpty())
	{
		FString DestPath, AssetName;
		if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("destination_path"), DestPath, Result.Errors, false) &&
			UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_name"), AssetName, Result.Errors, false))
		{
			AssetPath = FPaths::Combine(DestPath, AssetName);
		}
	}

	if (AssetPath.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing required parameter: 'asset_path' (or 'destination_path' and 'asset_name')."));
		return Result;
	}

	FString OutputFormatStr = TEXT("Stereo");
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("output_format"), OutputFormatStr, Result.Errors, false);

	int32 NumChannels = 0;
	if (UAgentFrameworkActionUtils::TryGetIntParam(Params, TEXT("num_channels"), NumChannels, Result.Errors, false))
	{
		switch (NumChannels)
		{
		case 1: OutputFormatStr = TEXT("Mono"); break;
		case 2: OutputFormatStr = TEXT("Stereo"); break;
		case 4: OutputFormatStr = TEXT("Quad"); break;
		case 6: OutputFormatStr = TEXT("5.1"); break;
		case 8: OutputFormatStr = TEXT("7.1"); break;
		default: break;
		}
	}

	EMetaSoundOutputAudioFormat FormatEnum = EMetaSoundOutputAudioFormat::Stereo;
	if (OutputFormatStr.Equals(TEXT("Mono"), ESearchCase::IgnoreCase))
	{
		FormatEnum = EMetaSoundOutputAudioFormat::Mono;
	}
	else if (OutputFormatStr.Equals(TEXT("Stereo"), ESearchCase::IgnoreCase))
	{
		FormatEnum = EMetaSoundOutputAudioFormat::Stereo;
	}
	else if (OutputFormatStr.Equals(TEXT("Quad"), ESearchCase::IgnoreCase))
	{
		FormatEnum = EMetaSoundOutputAudioFormat::Quad;
	}
	else if (OutputFormatStr.Equals(TEXT("5.1"), ESearchCase::IgnoreCase) || OutputFormatStr.Equals(TEXT("FiveDotOne"), ESearchCase::IgnoreCase))
	{
		FormatEnum = EMetaSoundOutputAudioFormat::FiveDotOne;
	}
	else if (OutputFormatStr.Equals(TEXT("7.1"), ESearchCase::IgnoreCase) || OutputFormatStr.Equals(TEXT("SevenDotOne"), ESearchCase::IgnoreCase))
	{
		FormatEnum = EMetaSoundOutputAudioFormat::SevenDotOne;
	}

	bool bIsPreset = false;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("is_preset"), bIsPreset, Result.Errors, false);
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("bIsPreset"), bIsPreset, Result.Errors, false);

	FString PresetSourcePath;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("preset_source_path"), PresetSourcePath, Result.Errors, false);

	UMetaSoundSource* ParentSource = nullptr;
	if (bIsPreset && !PresetSourcePath.IsEmpty())
	{
		ParentSource = LoadObject<UMetaSoundSource>(nullptr, *PresetSourcePath);
		if (!IsValid(ParentSource))
		{
			Result.Errors.Add(FString::Printf(TEXT("Parent MetaSoundSource asset not found at '%s' for preset creation."), *PresetSourcePath));
			return Result;
		}
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	FScopedTransaction Transaction(LOCTEXT("CreateMetaSoundSource", "Create MetaSound Source Asset"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UMetaSoundSourceFactory* Factory = NewObject<UMetaSoundSourceFactory>();
	if (!IsValid(Factory))
	{
		Result.Errors.Add(TEXT("Failed to create UMetaSoundSourceFactory."));
		return Result;
	}

	if (bIsPreset && IsValid(ParentSource))
	{
		Factory->ReferencedMetaSoundObject = ParentSource;
	}

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMetaSoundSource::StaticClass(), Factory);
	UMetaSoundSource* MetaSoundSource = Cast<UMetaSoundSource>(NewAsset);
	if (!IsValid(MetaSoundSource))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create MetaSoundSource asset at '%s'."), *AssetPath));
		return Result;
	}

	MetaSoundSource->OutputFormat = FormatEnum;
	MetaSoundSource->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully created MetaSoundSource asset '%s' (Format: %s, IsPreset: %s)."),
		*AssetPath, *OutputFormatStr, bIsPreset ? TEXT("True") : TEXT("False"));
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMetaSoundActions::ExecuteWireMetaSoundNodes(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UMetaSoundSource* MetaSoundSource = LoadObject<UMetaSoundSource>(nullptr, *AssetPath);
	if (!IsValid(MetaSoundSource))
	{
		Result.Errors.Add(FString::Printf(TEXT("MetaSoundSource asset not found at '%s'."), *AssetPath));
		return Result;
	}

	FScopedTransaction Transaction(LOCTEXT("WireMetaSoundNodes", "Wire MetaSound Nodes"));
	MetaSoundSource->Modify();

	TScriptInterface<IMetaSoundDocumentInterface> DocInterface(MetaSoundSource);
	FMetaSoundFrontendDocumentBuilder Builder(DocInterface);

	int32 NodesAddedCount = 0;
	int32 ConnectionsWiredCount = 0;

	TMap<FString, FGuid> NodeAliasToIDMap;

	// Collect existing nodes in document
	const FMetasoundFrontendGraph& BuildGraph = Builder.FindConstBuildGraphChecked();
	for (const FMetasoundFrontendNode& Node : BuildGraph.Nodes)
	{
		NodeAliasToIDMap.Add(Node.GetID().ToString(), Node.GetID());
		if (!Node.Name.IsNone())
		{
			NodeAliasToIDMap.Add(Node.Name.ToString(), Node.GetID());
		}
	}

	// 1. Process nodes_to_add / NodesToAdd
	const TArray<TSharedPtr<FJsonValue>>* NodesToAddArray = nullptr;
	if (Params->TryGetArrayField(TEXT("nodes_to_add"), NodesToAddArray) || Params->TryGetArrayField(TEXT("NodesToAdd"), NodesToAddArray))
	{
		if (NodesToAddArray)
		{
			for (const TSharedPtr<FJsonValue>& NodeVal : *NodesToAddArray)
			{
				const TSharedPtr<FJsonObject>* NodeObj = nullptr;
				if (!NodeVal.IsValid() || !NodeVal->TryGetObject(NodeObj) || !NodeObj || !NodeObj->IsValid())
				{
					continue;
				}

				FString NodeClassNameStr;
				FString NodeNameAlias;
				UAgentFrameworkActionUtils::TryGetStringParam(NodeObj->ToSharedRef(), TEXT("node_class_name"), NodeClassNameStr, Result.Errors, false);
				if (NodeClassNameStr.IsEmpty())
				{
					UAgentFrameworkActionUtils::TryGetStringParam(NodeObj->ToSharedRef(), TEXT("NodeClassName"), NodeClassNameStr, Result.Errors, false);
				}

				UAgentFrameworkActionUtils::TryGetStringParam(NodeObj->ToSharedRef(), TEXT("node_name"), NodeNameAlias, Result.Errors, false);
				if (NodeNameAlias.IsEmpty())
				{
					UAgentFrameworkActionUtils::TryGetStringParam(NodeObj->ToSharedRef(), TEXT("NodeName"), NodeNameAlias, Result.Errors, false);
				}

				if (NodeClassNameStr.IsEmpty())
				{
					continue;
				}

				FMetasoundFrontendClassName ClassName = ParseMetasoundClassName(NodeClassNameStr);
				const FMetasoundFrontendNode* NewNode = Builder.AddNodeByClassName(ClassName, 1);
				if (!NewNode)
				{
					// Try fallback with empty namespace
					FMetasoundFrontendClassName FallbackClassName(FName(), ClassName.Name, ClassName.Variant);
					NewNode = Builder.AddNodeByClassName(FallbackClassName, 1);
				}

				if (NewNode)
				{
					FGuid NodeID = NewNode->GetID();
					if (!NodeNameAlias.IsEmpty())
					{
						NodeAliasToIDMap.Add(NodeNameAlias, NodeID);
					}
					NodeAliasToIDMap.Add(NodeID.ToString(), NodeID);
					NodesAddedCount++;
				}
				else
				{
					Result.Warnings.Add(FString::Printf(TEXT("Failed to add MetaSound node for class '%s'."), *NodeClassNameStr));
				}
			}
		}
	}

	// 2. Process connections / ConnectionsToWire
	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray = nullptr;
	if (Params->TryGetArrayField(TEXT("connections"), ConnectionsArray) ||
		Params->TryGetArrayField(TEXT("ConnectionsToWire"), ConnectionsArray) ||
		Params->TryGetArrayField(TEXT("connections_to_wire"), ConnectionsArray))
	{
		if (ConnectionsArray)
		{
			for (const TSharedPtr<FJsonValue>& ConnVal : *ConnectionsArray)
			{
				const TSharedPtr<FJsonObject>* ConnObj = nullptr;
				if (!ConnVal.IsValid() || !ConnVal->TryGetObject(ConnObj) || !ConnObj || !ConnObj->IsValid())
				{
					continue;
				}

				FString FromNodeStr, FromPin, ToNodeStr, ToPin;
				UAgentFrameworkActionUtils::TryGetStringParam(ConnObj->ToSharedRef(), TEXT("from_node"), FromNodeStr, Result.Errors, false);
				if (FromNodeStr.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(ConnObj->ToSharedRef(), TEXT("FromNode"), FromNodeStr, Result.Errors, false);

				UAgentFrameworkActionUtils::TryGetStringParam(ConnObj->ToSharedRef(), TEXT("from_pin"), FromPin, Result.Errors, false);
				if (FromPin.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(ConnObj->ToSharedRef(), TEXT("FromPin"), FromPin, Result.Errors, false);

				UAgentFrameworkActionUtils::TryGetStringParam(ConnObj->ToSharedRef(), TEXT("to_node"), ToNodeStr, Result.Errors, false);
				if (ToNodeStr.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(ConnObj->ToSharedRef(), TEXT("ToNode"), ToNodeStr, Result.Errors, false);

				UAgentFrameworkActionUtils::TryGetStringParam(ConnObj->ToSharedRef(), TEXT("to_pin"), ToPin, Result.Errors, false);
				if (ToPin.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(ConnObj->ToSharedRef(), TEXT("ToPin"), ToPin, Result.Errors, false);

				if (FromNodeStr.IsEmpty() || FromPin.IsEmpty() || ToNodeStr.IsEmpty() || ToPin.IsEmpty())
				{
					continue;
				}

				FGuid FromNodeID, ToNodeID;
				if (const FGuid* FoundFrom = NodeAliasToIDMap.Find(FromNodeStr))
				{
					FromNodeID = *FoundFrom;
				}
				else
				{
					FGuid::Parse(FromNodeStr, FromNodeID);
				}

				if (const FGuid* FoundTo = NodeAliasToIDMap.Find(ToNodeStr))
				{
					ToNodeID = *FoundTo;
				}
				else
				{
					FGuid::Parse(ToNodeStr, ToNodeID);
				}

				if (!FromNodeID.IsValid() || !ToNodeID.IsValid())
				{
					Result.Warnings.Add(FString::Printf(TEXT("Could not resolve node GUIDs for connection from '%s' to '%s'."), *FromNodeStr, *ToNodeStr));
					continue;
				}

				Metasound::Frontend::FNamedEdge NamedEdge{ FromNodeID, FName(*FromPin), ToNodeID, FName(*ToPin) };
				bool bConnected = Builder.AddNamedEdges({ NamedEdge });
				if (!bConnected)
				{
					const FMetasoundFrontendVertex* OutputVertex = Builder.FindNodeOutput(FromNodeID, FName(*FromPin));
					const FMetasoundFrontendVertex* InputVertex = Builder.FindNodeInput(ToNodeID, FName(*ToPin));
					if (OutputVertex && InputVertex)
					{
						FMetasoundFrontendEdge Edge{ FromNodeID, OutputVertex->VertexID, ToNodeID, InputVertex->VertexID };
						Builder.AddEdge(MoveTemp(Edge));
						bConnected = true;
					}
				}

				if (bConnected)
				{
					ConnectionsWiredCount++;
				}
				else
				{
					Result.Warnings.Add(FString::Printf(TEXT("Failed to wire pin '%s' on node '%s' to pin '%s' on node '%s'."), *FromPin, *FromNodeStr, *ToPin, *ToNodeStr));
				}
			}
		}
	}

	Builder.FinishBuilding();
	MetaSoundSource->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully wired MetaSound graph at '%s': %d nodes added, %d connections wired."),
		*AssetPath, NodesAddedCount, ConnectionsWiredCount);
	return Result;
}

void FAgentFrameworkMetaSoundActions::PlaySuccessSound()
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

#undef LOCTEXT_NAMESPACE

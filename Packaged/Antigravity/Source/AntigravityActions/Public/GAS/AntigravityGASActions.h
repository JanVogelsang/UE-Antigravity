// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * FAntigravityGASActions
 *
 * Gameplay Ability System (GAS) tool pack. Provides 5 specialized tools that
 * let the AI build complete combat/ability systems:
 *
 *   - gas_register_tags: Inject gameplay tags into DefaultGameplayTags.ini
 *   - gas_create_attribute_set: Generate C++ AttributeSet with ATTRIBUTE_ACCESSORS boilerplate
 *   - gas_setup_asc: Add AbilitySystemComponent to a Blueprint actor (PlayerState or Character)
 *   - gas_create_effect: Create UGameplayEffect Blueprints with UE 5.3+ GEComponent routing
 *   - gas_create_ability: Create UGameplayAbility Blueprints with tags/cooldown/cost
 *
 * CRITICAL: UE 5.3+ Paradigm Shift
 *   In Unreal Engine 5.3, Epic refactored UGameplayEffect. Properties like GrantedTags,
 *   OngoingTagRequirements, GrantedAbilities, and Immunity were deprecated from the flat
 *   base class and moved into modular UGameplayEffectComponent subclasses:
 *     - UTargetTagsGameplayEffectComponent (grant tags to target)
 *     - UAssetTagsGameplayEffectComponent (tags the GE "owns")
 *     - UCancelAbilityTagsGameplayEffectComponent (cancel abilities)
 *     - UImmunityGameplayEffectComponent (immunity)
 *     - UAbilitiesGameplayEffectComponent (grant abilities)
 *
 *   The gas_create_effect tool automatically routes tag/ability grants through
 *   the correct GEComponent subclass rather than setting deprecated flat properties.
 *
 * EXECUTION ORDER (enforced via system prompt + AI task planner):
 *   1. gas_register_tags â€” Define all gameplay tags first
 *   2. gas_create_attribute_set â€” Generate C++ AttributeSet (requires compile)
 *   3. gas_setup_asc â€” Wire ASC to actor with AttributeSet registration
 *   4. gas_create_effect â€” Create GEs (Damage, Cooldown, Cost, Passive)
 *   5. gas_create_ability â€” Create GA Blueprints (then T3D inject the graphs)
 */
class ANTIGRAVITYACTIONS_API FAntigravityGASActions : public IAntigravityActionExecutor
{
public:
	FAntigravityGASActions();
	virtual ~FAntigravityGASActions();

	virtual FName GetActionName() const override;
	virtual FText GetDisplayName() const override;
	virtual EAntigravityActionCategory GetCategory() const override;
	virtual EAntigravityRiskLevel GetDefaultRiskLevel() const override;
	virtual FAntigravityActionPlan PreviewAction(const TSharedRef<FJsonObject>& Params) override;
	virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual bool CanUndo() const override;
	virtual bool UndoAction() override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/**
	 * Register gameplay tags in DefaultGameplayTags.ini via IGameplayTagsEditorModule.
	 * Tags become immediately available without editor restart.
	 */
	FAntigravityActionResult ExecuteRegisterTags(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/**
	 * Generate a C++ UAttributeSet subclass with FGameplayAttributeData members
	 * and ATTRIBUTE_ACCESSORS boilerplate macros. Requires a compile after creation.
	 */
	FAntigravityActionResult ExecuteCreateAttributeSet(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/**
	 * Add a UAbilitySystemComponent to a Blueprint actor's SCS, register
	 * AttributeSet subobjects, and optionally implement IAbilitySystemInterface.
	 */
	FAntigravityActionResult ExecuteSetupASC(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/**
	 * Create a UGameplayEffect Blueprint asset. Sets DurationPolicy, Modifiers,
	 * tags (via UE 5.3+ GEComponent routing), stacking, and cue tags.
	 */
	FAntigravityActionResult ExecuteCreateEffect(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/**
	 * Create a UGameplayAbility Blueprint asset. Sets instancing policy,
	 * activation tags, cooldown GE class, cost GE class, and net execution policy.
	 */
	FAntigravityActionResult ExecuteCreateAbility(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
};

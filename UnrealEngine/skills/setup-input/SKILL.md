---
name: setup-input
description: Configure the Enhanced Input system with Input Actions and Mapping Contexts.
---
# Skill: Setup Enhanced Input
## Description
Configure the Enhanced Input system with Input Actions and Mapping Contexts.
## Arguments
- {{arg}}: The action name to set up (e.g., Jump, Sprint, Interact)
## Steps
1. **Create Input Action** asset in Content/Input/Actions/IA_{{arg}}.uasset

2. **Add to Input Mapping Context** (create IMC_Default if it doesn't exist)

3. **In the character/pawn header**, add:
   ```cpp
   UPROPERTY(EditDefaultsOnly, Category="Input")
   TObjectPtr<UInputAction> {{arg}}Action;
   void Handle{{arg}}(const FInputActionValue& Value);
   ```

4. **In the character/pawn class**, override `SetupPlayerInputComponent` to bind the action:
   ```cpp
   // Header
   virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

   // Source
   void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
   {
       Super::SetupPlayerInputComponent(PlayerInputComponent);
       
       if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
       {
           EIC->BindAction({{arg}}Action, ETriggerEvent::Triggered, this, &AMyCharacter::Handle{{arg}});
       }
   }
   ```

5. **Implement the handler function** in C++:
   ```cpp
   void AMyCharacter::Handle{{arg}}(const FInputActionValue& Value)
   {
       // Retrieve input value (e.g. float or Vector2D depending on configuration)
       // float AxisValue = Value.Get<float>();
   }
   ```

6. **Blueprint Binding Alternative**:
   - In a Blueprint Character graph, search for the `EnhancedInputComponent` or right-click and search for `"EnhancedAction IA_{{arg}}"`.
   - Add the **Enhanced Action IA_{{arg}}** node.
   - Wire your game logic nodes to the **Triggered** pin.

## Notes
- Ensure `"EnhancedInput"` is added to `PublicDependencyModuleNames` in the project's `.Build.cs` file.
- Include `EnhancedInput/Public/EnhancedInputComponent.h`
- The project must have the EnhancedInput plugin enabled

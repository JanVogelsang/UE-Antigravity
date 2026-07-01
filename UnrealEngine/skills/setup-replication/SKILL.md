---
name: setup-replication
description: Configure an actor class for network replication in multiplayer UE games.
---
# Skill: Setup Actor Replication
## Description
Configure an actor class for network replication in multiplayer UE games.
## Arguments
- {{arg}}: Actor class to configure replication for
## Steps
1. **Constructor**: Enable replication
   ```cpp
   bReplicates = true;
   SetReplicateMovement(true);
   ```

2. **Add GetLifetimeReplicatedProps**:
   ```cpp
   void {{arg}}::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
   {
       Super::GetLifetimeReplicatedProps(OutLifetimeProps);
       DOREPLIFETIME({{arg}}, Health);
       DOREPLIFETIME_CONDITION({{arg}}, bIsVisible, COND_OwnerOnly);
   }
   ```

3. **Mark properties for replication**:
   
   **Standard Replication (No callback)**:
   ```cpp
   UPROPERTY(Replicated)
   float Health;
   ```
   
   **Replication with RepNotify Callback (Client side effect)**:
   ```cpp
   UPROPERTY(ReplicatedUsing=OnRep_Shield)
   float Shield;
   
   UFUNCTION()
   void OnRep_Shield(float OldShield);
   ```

4. **Server RPCs** for player actions:
   ```cpp
   UFUNCTION(Server, Reliable)
   void ServerDoAction();
   ```

5. **Required include**: `#include "Net/UnrealNetwork.h"`

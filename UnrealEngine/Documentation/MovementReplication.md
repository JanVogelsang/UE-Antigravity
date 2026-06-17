# Character Movement Replication Guide

To replicate custom movement in UE5, override PerformMovement and SendClientAdjustment in UCharacterMovementComponent.

Unreal Engine multiplayer movement replication utilizes a client-prediction and server-correction model. The client performs movement locally, sends the input to the server via ServerMove RPCs, and the server validates the move. If a correction is needed, the server sends a client adjustment.

// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityTransactionManager.h"
#include "AntigravityCoreModule.h"

FAntigravityTransactionManager::FAntigravityTransactionManager() {}
FAntigravityTransactionManager::~FAntigravityTransactionManager() {}
void FAntigravityTransactionManager::BeginUndoGroup(const FString& GroupName) { CurrentUndoGroup = GroupName; UndoGroupHistory.Add(GroupName); }
void FAntigravityTransactionManager::EndUndoGroup() { CurrentUndoGroup.Empty(); }
void FAntigravityTransactionManager::BeginTransaction(const FString& Description) { /* Stub: will use FScopedTransaction */ }
void FAntigravityTransactionManager::EndTransaction() { /* Stub */ }
bool FAntigravityTransactionManager::UndoLastGroup() { /* Stub */ return false; }

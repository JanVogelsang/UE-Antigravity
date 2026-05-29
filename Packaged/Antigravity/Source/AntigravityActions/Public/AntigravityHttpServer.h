// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "AntigravityActionRouter.h"

class FAntigravityHttpServer
{
public:
	static void Start();
	static void Stop();

private:
	static bool HandleExecuteToolRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	static bool HandleListToolsRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	static bool HandleGetSkillsRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	static void RegisterAllExecutors(TSharedRef<FAntigravityActionRouter> InRouter);

	static TSharedPtr<FAntigravityActionRouter> ActionRouter;
	static uint32 Port;
};

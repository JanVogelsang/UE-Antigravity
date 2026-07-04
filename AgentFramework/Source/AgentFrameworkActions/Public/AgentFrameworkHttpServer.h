// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "AgentFrameworkActionRouter.h"

class FAgentFrameworkHttpServer
{
public:
	static void Start();
	static void Stop();

private:
	static bool HandleExecuteToolRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	static bool HandleListToolsRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	static void RegisterAllExecutors(TSharedRef<FAgentFrameworkActionRouter> InRouter);

	static TSharedPtr<FAgentFrameworkActionRouter> ActionRouter;
	static uint32 Port;
};

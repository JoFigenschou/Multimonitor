#pragma once

#include "Modules/ModuleManager.h"

class FMultimonitorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

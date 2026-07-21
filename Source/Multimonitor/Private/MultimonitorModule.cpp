#include "MultimonitorModule.h"
#include "MultimonitorLog.h"

DEFINE_LOG_CATEGORY(LogMultimonitor);

#define LOCTEXT_NAMESPACE "FMultimonitorModule"

void FMultimonitorModule::StartupModule()
{
}

void FMultimonitorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMultimonitorModule, Multimonitor)

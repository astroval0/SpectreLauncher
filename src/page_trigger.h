#pragma once

#include "common.h"
#include "registry_utils.h"

bool RunPageTrigger(HANDLE processHandle, DWORD pid, const wstr& steamId);
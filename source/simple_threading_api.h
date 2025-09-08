#pragma once

#include "stdafx.h"
#include "script.h"

// Simple threading API functions
BIF_DECL(BIF_ThreadCreate);
BIF_DECL(BIF_ThreadDestroy);
BIF_DECL(BIF_ThreadCount);
BIF_DECL(BIF_ThreadSetVar);
BIF_DECL(BIF_ThreadGetVar);

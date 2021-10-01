#pragma once

#include "ff.application.h"
#include "ff.audio.h"
#include "ff.base.h"
#include "ff.data.h"
#include "ff.graphics.h"
#include "ff.input.h"
#include "ff.resource.h"
#include "ff.ui.h"

#if DXVER == 11
#include "ff.dx11.h"
#elif DXVER == 12
#include "ff.dx12.h"
#endif

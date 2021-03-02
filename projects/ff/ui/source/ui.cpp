#include "pch.h"
#include "ui.h"

static bool init_noesis()
{
    return true;
}

static void destroy_noesis()
{
}

bool ff::ui::internal::init()
{
    return ::init_noesis();
}

void ff::ui::internal::destroy()
{
    ::destroy_noesis();
}

#include "pch.h"
#include "ui.h"

static bool init_noesis()
{
    return true;
}

static void destroy_noesis()
{
}

bool ff::internal::ui::init()
{
    return ::init_noesis();
}

void ff::internal::ui::destroy()
{
    ::destroy_noesis();
}

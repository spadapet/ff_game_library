#include "pch.h"
#include "graphics_child_base.h"

int ff::internal::graphics_child_base::reset_priority() const
{
    return ff::internal::graphics_reset_priorities::normal;
}

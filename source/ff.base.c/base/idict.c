#include "pch.h"
#include "base/idict.h"

ff_idict_root ff_dict_pack(const ff_dict* dict, ff_arena* arena)
{
    (void)dict;
    (void)arena;
    return (ff_idict_root){ 0 };
}

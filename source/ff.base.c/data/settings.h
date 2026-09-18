#pragma once

#include "../base/string.h"
#include "../data/idict.h"

typedef struct ff_dict ff_dict;
typedef struct ff_signal ff_signal;

void ff_settings_init(ff_string_view settings_file);
void ff_settings_destroy(void);
void ff_settings_save(void);
ff_signal* ff_settings_save_signal(void);
ff_idict ff_settings_get(ff_string_view name);
ff_idict ff_settings_set(ff_string_view name, ff_dict* dict); // invalidates any ff_idict previously returned by ff_settings_get

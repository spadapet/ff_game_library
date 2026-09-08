#pragma once

// Internals shared between dict.c, idict.c and ivalue.c. These are not part of the public API; they
// live in a header only so the compiler checks that the definition and the uses agree, which a
// hand written extern declaration in each .c file does not.

typedef struct ff_dict ff_dict;
typedef struct ff_idict ff_idict;
typedef struct ff_ivalue ff_ivalue;
typedef struct ff_value ff_value;

// The values of a mutable dict, which follow its keys in the same allocation.
ff_value* internal_ff_dict_values(const ff_dict* dict);

// A pointer into a block's data section. 'offset' is relative to the start of that section and must
// already have been proven to sit inside it.
const void* internal_ff_idict_data(const ff_idict* dict, size_t offset);

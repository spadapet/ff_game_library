#pragma once

bool ff_math_is_pow2(size_t value);

// Smallest power of 2 that is >= value (returns 1 for value <= 1). For values whose next
// power of 2 would overflow size_t, returns value unchanged.
size_t ff_math_round_up_pow2(size_t value);

// Round value up to the next multiple of alignment. alignment must be a power of 2.
size_t ff_math_round_up(size_t value, size_t alignment);

// Round a pointer up to the next address aligned to alignment. alignment must be a power of 2.
uint8_t* ff_math_align_up(uint8_t* ptr, size_t alignment);

static inline size_t ff_math_min_size(size_t l, size_t r) { return l < r ? l : r; }
static inline int ff_math_min_int(int l, int r) { return l < r ? l : r; }
static inline float ff_math_min_float(float l, float r) { return l < r ? l : r; }
static inline double ff_math_min_double(double l, double r) { return l < r ? l : r; }

static inline size_t ff_math_max_size(size_t l, size_t r) { return l > r ? l : r; }
static inline int ff_math_max_int(int l, int r) { return l > r ? l : r; }
static inline float ff_math_max_float(float l, float r) { return l > r ? l : r; }
static inline double ff_math_max_double(double l, double r) { return l > r ? l : r; }

#include "pch.h"

// Tests for ff::hash_bytes / ff::hash_string / ff::hash_data (a streaming wyhash variant).
// Coverage:
//   * Correctness: determinism, string/byte equivalence, and the core invariant that a
//     streamed hash equals the one-shot hash no matter how the bytes are split.
//   * Quality for dictionary keys: single-bit-flip avalanche (~half of the 64 output bits
//     change) and low-bit uniformity, so a power-of-two table indexed with hash & (cap - 1)
//     stays well distributed.
//   * Stability: fixed "golden" values, since the hash is documented as stable across
//     runs/builds and safe to persist.

static int popcount64(uint64_t value)
{
    int count = 0;
    while (value)
    {
        value &= value - 1;
        count++;
    }
    return count;
}

static int hamming64(uint64_t a, uint64_t b)
{
    return popcount64(a ^ b);
}

// Fill 'buffer' with a deterministic, byte-varying pattern.
static void make_pattern(uint8_t* buffer, size_t size, uint8_t seed)
{
    uint8_t value = seed;
    for (size_t i = 0; i < size; i++)
    {
        value = (uint8_t)(value * 31u + 17u + (uint8_t)i);
        buffer[i] = value;
    }
}

static int compare_u64(const void* a, const void* b)
{
    uint64_t x = *(const uint64_t*)a;
    uint64_t y = *(const uint64_t*)b;
    return (x < y) ? -1 : ((x > y) ? 1 : 0);
}

// Streamed hash of a single buffer split into two calls at 'split'.
static uint64_t hash_two_parts(const uint8_t* p, size_t size, size_t split)
{
    ff::hash_data state;
    state.init();
    state.hash(p, split);
    state.hash(p + split, size - split);
    return state.done();
}

namespace ff::test::base
{
    TEST_CLASS(hash_tests)
    {
    public:
        // ====================================================================
        // Correctness
        // ====================================================================
        TEST_METHOD(hash_bytes_is_deterministic)
        {
            uint8_t buffer[64];
            make_pattern(buffer, sizeof(buffer), 0x5A);
            Assert::AreEqual<uint64_t>(ff::hash_bytes(buffer, sizeof(buffer)), ff::hash_bytes(buffer, sizeof(buffer)));
        }

        TEST_METHOD(hash_string_matches_hash_bytes)
        {
            const char text[] = "dictionary key";
            Assert::AreEqual<uint64_t>(ff::hash_bytes(text, sizeof(text) - 1), ff::hash_string(FF_SVL("dictionary key")));
        }

        TEST_METHOD(hash_wstring_matches_hash_bytes)
        {
            const wchar_t text[] = L"dictionary key";
            Assert::AreEqual<uint64_t>(ff::hash_bytes(text, sizeof(text) - sizeof(wchar_t)), ff::hash_string(FF_WSVL(L"dictionary key")));
        }

        TEST_METHOD(narrow_and_wide_hash_differ)
        {
            // Same characters, different byte width -> different bytes -> different hashes.
            Assert::AreNotEqual<uint64_t>(ff::hash_string(FF_SVL("hello")), ff::hash_string(FF_WSVL(L"hello")));
        }

        TEST_METHOD(empty_input_is_stable_and_consistent)
        {
            uint64_t a = ff::hash_bytes(nullptr, 0);
            uint64_t b = ff::hash_bytes("ignored", 0);
            uint64_t c = ff::hash_string(FF_SVL(""));

            ff::hash_data state;
            state.init();
            uint64_t d = state.done(); // no hash() calls at all

            Assert::AreEqual<uint64_t>(a, b);
            Assert::AreEqual<uint64_t>(a, c);
            Assert::AreEqual<uint64_t>(a, d);
        }

        TEST_METHOD(distinct_short_strings_differ)
        {
            uint64_t h_a = ff::hash_string(FF_SVL("a"));
            uint64_t h_b = ff::hash_string(FF_SVL("b"));
            uint64_t h_ab = ff::hash_string(FF_SVL("ab"));
            uint64_t h_ba = ff::hash_string(FF_SVL("ba"));

            Assert::AreNotEqual<uint64_t>(h_a, h_b);
            Assert::AreNotEqual<uint64_t>(h_a, h_ab);
            Assert::AreNotEqual<uint64_t>(h_ab, h_ba); // order sensitive
        }

        // ====================================================================
        // Streaming invariant: streamed hash == one-shot hash
        // ====================================================================
        TEST_METHOD(streamed_equals_one_shot_at_every_split)
        {
            uint8_t buffer[100];
            make_pattern(buffer, sizeof(buffer), 0x13);
            uint64_t one_shot = ff::hash_bytes(buffer, sizeof(buffer));

            for (size_t split = 0; split <= sizeof(buffer); split++)
            {
                Assert::AreEqual<uint64_t>(one_shot, hash_two_parts(buffer, sizeof(buffer), split),
                    L"two-call streamed hash differed from one-shot");
            }
        }

        TEST_METHOD(streamed_equals_one_shot_byte_by_byte)
        {
            const size_t lengths[] = { 1, 7, 8, 15, 16, 17, 31, 32, 33, 64, 100 };
            uint8_t buffer[100];
            make_pattern(buffer, sizeof(buffer), 0x6E);

            for (size_t li = 0; li < sizeof(lengths) / sizeof(lengths[0]); li++)
            {
                size_t len = lengths[li];
                uint64_t one_shot = ff::hash_bytes(buffer, len);

                ff::hash_data state;
                state.init();
                for (size_t i = 0; i < len; i++)
                {
                    state.hash(buffer + i, 1);
                }
                Assert::AreEqual<uint64_t>(one_shot, state.done(), L"byte-by-byte streamed hash differed from one-shot");
            }
        }

        TEST_METHOD(streamed_equals_one_shot_across_block_boundaries)
        {
            // Lengths around the 16-byte block boundary exercise the held-final-block logic.
            const size_t lengths[] = { 15, 16, 17, 31, 32, 33, 47, 48, 49 };
            uint8_t buffer[64];
            make_pattern(buffer, sizeof(buffer), 0xC4);

            for (size_t li = 0; li < sizeof(lengths) / sizeof(lengths[0]); li++)
            {
                size_t len = lengths[li];
                uint64_t one_shot = ff::hash_bytes(buffer, len);
                for (size_t split = 0; split <= len; split++)
                {
                    Assert::AreEqual<uint64_t>(one_shot, hash_two_parts(buffer, len, split));
                }
            }
        }

        TEST_METHOD(streamed_equals_one_shot_three_chunks)
        {
            uint8_t buffer[70];
            make_pattern(buffer, sizeof(buffer), 0x3B);
            uint64_t one_shot = ff::hash_bytes(buffer, sizeof(buffer));

            for (size_t a = 0; a <= sizeof(buffer); a += 5)
            {
                for (size_t b = a; b <= sizeof(buffer); b += 7)
                {
                    ff::hash_data state;
                    state.init();
                    state.hash(buffer, a);
                    state.hash(buffer + a, b - a);
                    state.hash(buffer + b, sizeof(buffer) - b);
                    Assert::AreEqual<uint64_t>(one_shot, state.done());
                }
            }
        }

        // ====================================================================
        // Quality: avalanche and low-bit mixing for dictionary keys
        // ====================================================================
        TEST_METHOD(single_bit_flip_avalanches_output)
        {
            uint8_t buffer[24];
            make_pattern(buffer, sizeof(buffer), 0x91);
            uint64_t base = ff::hash_bytes(buffer, sizeof(buffer));

            int total_bits = (int)sizeof(buffer) * 8; // 192 input bits
            int sum_hamming = 0;
            for (int bit = 0; bit < total_bits; bit++)
            {
                buffer[bit / 8] ^= (uint8_t)(1u << (bit % 8));
                uint64_t flipped = ff::hash_bytes(buffer, sizeof(buffer));
                buffer[bit / 8] ^= (uint8_t)(1u << (bit % 8)); // restore

                int hd = hamming64(base, flipped);
                // A single input-bit change must flip a large, well-spread set of output bits.
                // The wide band only catches catastrophic non-mixing; the average below is the
                // precise gate.
                Assert::IsTrue(hd >= 6 && hd <= 58, L"single-bit flip produced poor avalanche");
                sum_hamming += hd;
            }

            // Ideal avalanche flips ~32 of 64 bits on average. Averaging 192 samples makes this tight.
            double average = (double)sum_hamming / total_bits;
            Assert::IsTrue(average >= 29.0 && average <= 35.0, L"average avalanche far from the ideal 32 bits");
        }

        TEST_METHOD(low_bits_uniform_for_dictionary_keys)
        {
            // Bucket many realistic keys by their low 8 bits (what hash & (cap - 1) uses for a
            // 256-slot table). A well-mixed low byte keeps every bucket within a generous band.
            const int key_count = 8192;
            const int bucket_count = 256;
            int buckets[256];
            for (int i = 0; i < bucket_count; i++)
            {
                buckets[i] = 0;
            }

            for (int i = 0; i < key_count; i++)
            {
                char key[64];
                int n = sprintf_s(key, "asset/texture_%d.png", i);
                uint64_t h = ff::hash_bytes(key, (size_t)n);
                buckets[h & (bucket_count - 1)]++;
            }

            int expected = key_count / bucket_count; // 32 per bucket
            int min_count = buckets[0];
            int max_count = buckets[0];
            for (int i = 0; i < bucket_count; i++)
            {
                if (buckets[i] < min_count) min_count = buckets[i];
                if (buckets[i] > max_count) max_count = buckets[i];
            }

            Assert::IsTrue(min_count >= expected / 4, L"a low-bit bucket was starved (poor low-bit mixing)");
            Assert::IsTrue(max_count <= expected * 3, L"a low-bit bucket was overloaded (poor low-bit mixing)");
        }

        TEST_METHOD(sequential_keys_scatter_low_bits)
        {
            // Keys that differ only in a trailing counter must still scatter across low-bit
            // buckets; clustering would mean the low bits barely respond to small input changes.
            const int key_count = 1000;
            const int bucket_count = 1024;
            bool used[1024];
            for (int i = 0; i < bucket_count; i++)
            {
                used[i] = false;
            }

            for (int i = 0; i < key_count; i++)
            {
                char key[32];
                int n = sprintf_s(key, "node_%d", i);
                uint64_t h = ff::hash_bytes(key, (size_t)n);
                used[h & (bucket_count - 1)] = true;
            }

            int occupied = 0;
            for (int i = 0; i < bucket_count; i++)
            {
                if (used[i]) occupied++;
            }

            // 1000 well-mixed values into 1024 buckets occupy ~635 on average.
            Assert::IsTrue(occupied >= 550, L"sequential keys clustered into too few low-bit buckets");
        }

        TEST_METHOD(one_character_difference_changes_many_bits)
        {
            uint64_t h1 = ff::hash_string(FF_SVL("player_health"));
            uint64_t h2 = ff::hash_string(FF_SVL("player_healht")); // last two chars transposed
            uint64_t h3 = ff::hash_string(FF_SVL("player_health ")); // one extra byte

            Assert::AreNotEqual<uint64_t>(h1, h2);
            Assert::AreNotEqual<uint64_t>(h1, h3);
            Assert::IsTrue(hamming64(h1, h2) >= 12, L"transposed characters barely changed the hash");
            Assert::IsTrue(hamming64(h1, h3) >= 12, L"appending a byte barely changed the hash");
        }

        // ====================================================================
        // Collisions and length/tail handling
        // ====================================================================
        TEST_METHOD(no_collisions_across_many_keys)
        {
            const int key_count = 20000;
            uint64_t* hashes = (uint64_t*)malloc((size_t)key_count * sizeof(uint64_t));
            Assert::IsNotNull(hashes);

            for (int i = 0; i < key_count; i++)
            {
                char key[64];
                int n = sprintf_s(key, "entity:%d:component", i);
                hashes[i] = ff::hash_bytes(key, (size_t)n);
            }

            qsort(hashes, (size_t)key_count, sizeof(uint64_t), compare_u64);

            int collisions = 0;
            for (int i = 1; i < key_count; i++)
            {
                if (hashes[i] == hashes[i - 1]) collisions++;
            }
            free(hashes);

            Assert::AreEqual(0, collisions, L"unexpected 64-bit hash collisions among distinct keys");
        }

        TEST_METHOD(length_changes_hash)
        {
            // Same repeated byte, increasing length: the folded-in total length must make
            // every length hash differently.
            uint8_t buffer[64];
            for (int i = 0; i < 64; i++)
            {
                buffer[i] = 0x77;
            }

            uint64_t hashes[65];
            for (int len = 1; len <= 64; len++)
            {
                hashes[len] = ff::hash_bytes(buffer, (size_t)len);
            }
            for (int a = 1; a <= 64; a++)
            {
                for (int b = a + 1; b <= 64; b++)
                {
                    Assert::AreNotEqual<uint64_t>(hashes[a], hashes[b], L"two lengths of the same byte collided");
                }
            }
        }

        TEST_METHOD(finalize_tail_branches_produce_distinct_hashes)
        {
            // Lengths 1..16 exercise every finalize tail path (1..3, 4..7, 8..16).
            uint8_t buffer[16];
            make_pattern(buffer, sizeof(buffer), 0x2C);

            uint64_t hashes[17];
            for (int len = 1; len <= 16; len++)
            {
                hashes[len] = ff::hash_bytes(buffer, (size_t)len);
            }
            for (int a = 1; a <= 16; a++)
            {
                for (int b = a + 1; b <= 16; b++)
                {
                    Assert::AreNotEqual<uint64_t>(hashes[a], hashes[b]);
                }
            }
        }

        TEST_METHOD(zero_filled_buffers_are_mixed)
        {
            // All-zero inputs are a common degenerate case; different lengths must still
            // produce distinct, non-zero hashes.
            uint8_t buffer[40];
            for (int i = 0; i < 40; i++)
            {
                buffer[i] = 0;
            }

            uint64_t hashes[41];
            for (int len = 1; len <= 40; len++)
            {
                hashes[len] = ff::hash_bytes(buffer, (size_t)len);
                Assert::AreNotEqual<uint64_t>(0ull, hashes[len], L"all-zero input hashed to zero");
            }
            for (int a = 1; a <= 40; a++)
            {
                for (int b = a + 1; b <= 40; b++)
                {
                    Assert::AreNotEqual<uint64_t>(hashes[a], hashes[b]);
                }
            }
        }

        // ====================================================================
        // Stability (golden values): captured below, then locked.
        // ====================================================================
        TEST_METHOD(known_hash_values_are_stable)
        {
            // Golden values lock the documented "stable across runs/builds, safe to persist"
            // guarantee. If the algorithm or its secret constants ever change, these must be
            // updated deliberately (and any persisted hashes invalidated).
            Assert::AreEqual<uint64_t>(0x0409638EE2BDE459ull, ff::hash_bytes("", 0));
            Assert::AreEqual<uint64_t>(0x28D2053309D28531ull, ff::hash_string(FF_SVL("a")));
            Assert::AreEqual<uint64_t>(0x0E24BBD9F93F532Dull, ff::hash_string(FF_SVL("hello")));
            Assert::AreEqual<uint64_t>(0x668D5E431C3B2573ull, ff::hash_string(FF_SVL("hello world")));
            Assert::AreEqual<uint64_t>(0xC304E72C387CD229ull, ff::hash_string(FF_SVL("0123456789abcdef")));
            Assert::AreEqual<uint64_t>(0xEACB223E285616F0ull, ff::hash_string(FF_SVL("0123456789abcdefg")));
            Assert::AreEqual<uint64_t>(0xF88C518CF1EFA0BAull, ff::hash_string(FF_SVL("The quick brown fox jumps over the lazy dog")));
        }
    };
}

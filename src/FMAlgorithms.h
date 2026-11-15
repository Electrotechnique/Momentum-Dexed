/*
  The 32 DX7 algorithms: which operator modulates which, and which has the feedback loop.
  Used to find the carriers and to draw the algorithm diagram.
*/
#ifndef MOMENTUM_FM_ALGORITHMS_H
#define MOMENTUM_FM_ALGORITHMS_H

#include <stdint.h>

#define FM_ALGORITHMS 32
#define FM_MAX_LINKS 5

typedef struct FMAlgorithm
{
    uint8_t feedbackOp;            // Operator with the feedback loop (1-6)
    uint8_t links;                 // Number of modulator -> target links
    uint8_t mod[FM_MAX_LINKS][2];  // {modulator, target}, operators 1-6
} FMAlgorithm;

const FMAlgorithm FM_ALGORITHM[FM_ALGORITHMS] = {
    {6, 4, {{2, 1}, {6, 5}, {5, 4}, {4, 3}}},          // 1
    {2, 4, {{2, 1}, {6, 5}, {5, 4}, {4, 3}}},          // 2
    {6, 4, {{3, 2}, {2, 1}, {6, 5}, {5, 4}}},          // 3
    {6, 4, {{3, 2}, {2, 1}, {6, 5}, {5, 4}}},          // 4 (loop from OP4 to OP6)
    {6, 3, {{2, 1}, {4, 3}, {6, 5}}},                  // 5
    {6, 3, {{2, 1}, {4, 3}, {6, 5}}},                  // 6 (loop from OP5 to OP6)
    {6, 4, {{2, 1}, {4, 3}, {5, 3}, {6, 5}}},          // 7
    {4, 4, {{2, 1}, {4, 3}, {5, 3}, {6, 5}}},          // 8
    {2, 4, {{2, 1}, {4, 3}, {5, 3}, {6, 5}}},          // 9
    {3, 4, {{3, 2}, {2, 1}, {5, 4}, {6, 4}}},          // 10
    {6, 4, {{3, 2}, {2, 1}, {5, 4}, {6, 4}}},          // 11
    {2, 4, {{2, 1}, {4, 3}, {5, 3}, {6, 3}}},          // 12
    {6, 4, {{2, 1}, {4, 3}, {5, 3}, {6, 3}}},          // 13
    {6, 4, {{2, 1}, {4, 3}, {5, 4}, {6, 4}}},          // 14
    {2, 4, {{2, 1}, {4, 3}, {5, 4}, {6, 4}}},          // 15
    {6, 5, {{2, 1}, {3, 1}, {4, 3}, {5, 1}, {6, 5}}},  // 16
    {2, 5, {{2, 1}, {3, 1}, {4, 3}, {5, 1}, {6, 5}}},  // 17
    {3, 5, {{2, 1}, {3, 1}, {4, 1}, {5, 4}, {6, 5}}},  // 18
    {6, 4, {{3, 2}, {2, 1}, {6, 4}, {6, 5}}},          // 19
    {3, 4, {{3, 1}, {3, 2}, {5, 4}, {6, 4}}},          // 20
    {3, 4, {{3, 1}, {3, 2}, {6, 4}, {6, 5}}},          // 21
    {6, 4, {{2, 1}, {6, 3}, {6, 4}, {6, 5}}},          // 22
    {6, 3, {{3, 2}, {6, 4}, {6, 5}}},                  // 23
    {6, 3, {{6, 3}, {6, 4}, {6, 5}}},                  // 24
    {6, 2, {{6, 4}, {6, 5}}},                          // 25
    {6, 3, {{3, 2}, {5, 4}, {6, 4}}},                  // 26
    {3, 3, {{3, 2}, {5, 4}, {6, 4}}},                  // 27
    {5, 3, {{2, 1}, {5, 4}, {4, 3}}},                  // 28
    {6, 2, {{4, 3}, {6, 5}}},                          // 29
    {5, 2, {{5, 4}, {4, 3}}},                          // 30
    {6, 1, {{6, 5}}},                                  // 31
    {6, 0, {}},                                        // 32
};

// Carrier operators of algorithm 0-31 (those modulating nothing), bit (n-1) = OP n
inline uint8_t fmCarrierMask(uint8_t algorithm)
{
    const FMAlgorithm &a = FM_ALGORITHM[algorithm % FM_ALGORITHMS];
    uint8_t modulators = 0;
    for (uint8_t i = 0; i < a.links; i++)
        modulators |= 1 << (a.mod[i][0] - 1);
    return 0x3F & ~modulators;
}

#endif

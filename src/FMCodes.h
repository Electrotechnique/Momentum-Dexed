/*
  Momentum FM: encoder codes and ids of the FM parameters (see FMEdit.h)
*/
#ifndef MOMENTUM_FM_CODES_H
#define MOMENTUM_FM_CODES_H

#define fmOpSelect 194     // Operator 1-6
#define fmStageSelect 195  // Operator envelope stage 1-4
#define fmPStageSelect 196 // Pitch envelope stage 1-4
#define syxPage 197        // Edit Banks: open the DX7 .syx page
#define syxImport 198      // .syx page: choose a file / import it
#define syxExport 199      // .syx page: export the bank
#define FM_PARAM_BASE 200

enum FMParamId
{
    FMP_MODE,
    FMP_COARSE,
    FMP_FINE,
    FMP_DETUNE,
    FMP_LEVEL,
    FMP_ON,
    FMP_VELSENS,
    FMP_AMS,
    FMP_RATESCALE,
    FMP_EGRATE,
    FMP_EGLEVEL,
    FMP_BREAKPT,
    FMP_LDEPTH,
    FMP_RDEPTH,
    FMP_LCURVE,
    FMP_RCURVE,
    FMP_ALGORITHM,
    FMP_FEEDBACK,
    FMP_OSCSYNC,
    FMP_TRANSPOSE,
    FMP_PEGRATE,
    FMP_PEGLEVEL,
    FMP_LFOSPEED,
    FMP_LFODELAY,
    FMP_LFOPMD,
    FMP_LFOAMD,
    FMP_LFOSYNC,
    FMP_LFOWAVE,
    FMP_LFOPMS,
    FMP_MWRANGE,
    FMP_MWTARGET,
    FMP_ATRANGE,
    FMP_ATTARGET,
    FMP_ENGINE,
    FMP_MACRO_ATTACK, // Envelope macros: rate 1-4 of all operators
    FMP_MACRO_DECAY,
    FMP_MACRO_DECAY2,
    FMP_MACRO_RELEASE,
    FMP_COUNT
};

inline bool isFMParam(uint8_t code) { return code >= FM_PARAM_BASE && code < FM_PARAM_BASE + FMP_COUNT; }

#endif

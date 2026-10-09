/* The one translation unit that compiles the dr_libs implementations
   (dr_wav, dr_mp3, dr_flac: public domain or MIT-0, by David Reid). Every
   other file includes the headers for their declarations only.

   The implementation macros are the DR_*_IMPLEMENTATION spellings: dr_wav
   and dr_flac still accept DRWAV_ and DRFLAC_, but dr_mp3 recognises only
   DR_MP3_IMPLEMENTATION. The build compiles this file without warnings, as
   it is third-party code. */
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

/*
 * aether_lib.i — Per-project SWIG interface.
 *
 * The user's .ae file exports aether_<name>(...) entry points. For SWIG to
 * wrap them, a small header describing those signatures must exist. In a
 * real project you'd maintain this header; for the test we inline the
 * declarations directly in the .i file.
 */

%module aether_lib

%{
#include <stdint.h>
extern int32_t aether_add(int32_t a, int32_t b);
extern const char* aether_shout(const char* s);
%}

%include <stdint.i>

int32_t aether_add(int32_t a, int32_t b);
const char* aether_shout(const char* s);

/*
 * aether_config.i — SWIG interface for the Aether embedding ABI.
 *
 * Generates language bindings for any SWIG target. Typical invocations:
 *
 *   swig -python  -o aether_config_wrap.c aether_config.i
 *   swig -java    -package com.example.aether aether_config.i
 *   swig -ruby    aether_config.i
 *   swig -go      aether_config.i
 *
 * The binding then consumes the shared library produced by
 * `aetherc --emit=lib` plus libaether_config.a.
 *
 * Notes on opacity:
 *   - AetherValue is a forward-declared struct; SWIG generates a proxy
 *     class for it in each target language (Python `AetherValue`,
 *     Java `AetherValue`, etc.).
 *   - The proxy is never instantiated by the target language; users
 *     receive handles from the generated `aether_<name>` entry points
 *     and pass them into accessors.
 *
 * Notes on ownership:
 *   - %newobject marks aether_config_* functions whose return value the
 *     caller owns. SWIG then emits language-appropriate finalizers.
 *   - However, because most of our accessors return *borrowed* handles
 *     (nested map/list tied to the root's lifetime), we DO NOT mark
 *     them %newobject. Only the entry-point functions from the user's
 *     script are Owned — but those are named by the user at codegen
 *     time, so we can't mark them here. Callers should hand-edit the
 *     generated .i or invoke aether_config_free() explicitly.
 */

%module aether_config

%{
#include "aether_config.h"
%}

/* String returns are borrowed pointers into the parent tree. SWIG's
 * default `const char*` typemap copies to a target-language string,
 * which is the behavior we want — the copy decouples the target from
 * the Aether-owned storage. */

/* Mark the explicit free function so SWIG knows it's a destructor. */
%delobject aether_config_free;

/* Include the header verbatim so SWIG sees every declaration. */
%include "aether_config.h"

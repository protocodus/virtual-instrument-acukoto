#pragma once

// These internal tests each compile the engine once in their sole translation
// unit and do not link AcukotoDSP. GCC normally exempts types defined in the
// primary .cpp from -Wsubobject-linkage, but including that same implementation
// enables its cross-translation-unit diagnostic. GCC 13 crashes while checking
// State's anonymous-namespace member types (decl_anon_ns_mem_p), before it
// checks whether the warning is enabled. Its constrain_class_visibility skips
// system-header declarations, so mark only this test implementation include.
// The ordinary DSP and C++17 targets still compile the engine with their full
// warnings; the test bodies following this header retain normal diagnostics.
// No assertions, production linkage or production compilation are changed.
// See GCC's -Wsubobject-linkage documentation and GCC 13 implementation:
// https://gcc.gnu.org/onlinedocs/gcc/C_002b_002b-Dialect-Options.html
// https://github.com/gcc-mirror/gcc/blob/releases/gcc-13.3.0/gcc/cp/decl2.cc
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC system_header
#endif

#include "../Source/DSP/AcukotoEngine.cpp"

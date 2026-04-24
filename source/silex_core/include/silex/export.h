#pragma once

/// @file export.h
/// @brief DLL export/import macros for the Silex public API.

#ifdef _WIN32
  #ifdef SILEX_CORE_EXPORTS
    #define SILEX_API __declspec(dllexport)
  #else
    #define SILEX_API __declspec(dllimport)
  #endif
#else
  #define SILEX_API __attribute__((visibility("default")))
#endif

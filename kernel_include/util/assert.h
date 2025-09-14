#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern void(*___assert_func)(const char* condition, const char* file, int line, const char* message);

#define ASSERT(condition, message) if (!(condition)) ___assert_func(#condition, __FILE__, __LINE__, message)

#ifdef __cplusplus
}
#endif
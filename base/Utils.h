/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#ifndef UTILS_H_
#define UTILS_H_

#define TIME(funcname, func)                                                   \
  {                                                                            \
    clock_t begin = clock();                                                   \
    func;                                                                      \
    clock_t end = clock();                                                     \
    double time_spent = (double)(end - begin) / (CLOCKS_PER_SEC / 1000);       \
    printf("Time for %s = %fms\n", funcname, time_spent);                      \
  }

#define TIMEWITHSIZE(funcname, func, size)                                     \
  {                                                                            \
    clock_t begin = clock();                                                   \
    func;                                                                      \
    clock_t end = clock();                                                     \
    double time_spent = (double)(end - begin) / (CLOCKS_PER_SEC / 1000);       \
    printf("Time for %s %d = %fms\n", funcname, size, time_spent);             \
  }

#endif
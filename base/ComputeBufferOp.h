/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#ifndef COMPUTE_BUFFER_OP_H_
#define COMPUTE_BUFFER_OP_H_
#include "ComputeOp.h"

class ComputeBufferOp : public ComputeOp {
public:
  ComputeBufferOp();
  ComputeBufferOp(const InitParams &init_params);
  void execute();
  virtual ~ComputeBufferOp();
};
#endif
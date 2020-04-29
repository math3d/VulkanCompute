/*
* Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/
#ifndef COMPUTE_BUFFER_TO_IMAGE_OP_H_
#define COMPUTE_BUFFER_TO_IMAGE_OP_H_
#include "ComputeOp.h"


class ComputeBufferToImageOp: public ComputeOp{
public:
  ComputeBufferToImageOp();
  ComputeBufferToImageOp(const InitParams& init_params);
  void execute();
  virtual ~ComputeBufferToImageOp();
};
#endif
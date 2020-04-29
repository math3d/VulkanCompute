/*
* Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/
#ifndef COMPUTE_IMAGE_OP_H_
#define COMPUTE_IMAGE_OP_H_
#include "ComputeOp.h"


class ComputeImageOp: public ComputeOp{
public:
  ComputeImageOp();
  ComputeImageOp(const InitParams& init_params);
  void execute();
  virtual ~ComputeImageOp();
};
#endif
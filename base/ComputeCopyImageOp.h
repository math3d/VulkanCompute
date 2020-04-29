/*
* Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/
#ifndef COMPUTE_COPY_IMAGE_OP_H_
#define COMPUTE_COPY_IMAGE_OP_H_
#include "ComputeOp.h"

template<typename T>
class ComputeCopyImageOp: public ComputeOp<T>{
public:
  ComputeCopyImageOp();
  ComputeCopyImageOp(const InitParams<T>& init_params);
  void execute();
  virtual ~ComputeCopyImageOp();
};
#endif
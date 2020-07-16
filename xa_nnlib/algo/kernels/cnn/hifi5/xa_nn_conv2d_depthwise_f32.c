/*******************************************************************************
* Copyright (c) 2018-2020 Cadence Design Systems, Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to use this Software with Cadence processor cores only and
* not with any other processors and platforms, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

******************************************************************************/
#include "common_fpu.h"
#include "xa_type_def.h"
#include "xa_nnlib_kernels_api.h"
#include "xa_nn_conv2d_depthwise_state.h"
#include "xa_nnlib_common_macros_hifi5.h"
#include "xa_nnlib_err_chk.h"

#if !HAVE_VFPU
DISCARD_FUN_FOR_NONVOID_RETURN(WORD32, xa_nn_conv2d_depthwise_f32,(
    FLOAT32* __restrict__ p_out,
    const FLOAT32* __restrict__ p_kernel,
    const FLOAT32* __restrict__ p_inp,
    const FLOAT32* __restrict__ p_bias,
    WORD32  input_height,
    WORD32  input_width,
    WORD32  input_channels,
    WORD32  kernel_height,
    WORD32  kernel_width,
    WORD32  channels_multiplier,
    WORD32  x_stride,
    WORD32  y_stride,
    WORD32  x_padding,
    WORD32  y_padding,
    WORD32  out_height,
    WORD32  out_width,
    WORD32  inp_data_format,
    WORD32  out_data_format,
    pVOID p_scratch))
#else /* #if !HAVE_VFPU */
static void convolve_f32(
    FLOAT32*  __restrict__ p_out,
    const FLOAT32* __restrict__ p_ker,
    const FLOAT32* __restrict__ p_inp,
    const FLOAT32* __restrict__ p_bias,
    WORD32   input_width,
    WORD32   kernel_height,
    WORD32   kernel_width,
    WORD32   x_stride,
    WORD32   y_stride,
    WORD32   out_height,
    WORD32   out_width,
    WORD32   out_stride,
    pVOID    p_scratch)
{
    int itr_oh, itr_ow, itr_kh, itr_kw;
    int total_out_width = (input_width - kernel_width) + 1;
    int kernel_width_pad = ALIGNED_SIZE(kernel_width, 4);
    xtfloatx4 *ptr_inp;
    xtfloatx4 *ptr_ker;
    xtfloatx2 *ptr_out;

    xtfloatx2 ker0, ker1, ker2, ker3, ker4, ker5;
    xtfloatx2 accu_x2_0, accu_x2_1;
    xtfloatx2 accu_x2_0_a, accu_x2_1_a;
    xtfloatx2 accu_x2_0_b, accu_x2_1_b;
    xtfloatx2 accu_x2_0_c, accu_x2_1_c;
    xtfloatx2 id4, id8, id12, id16, id20, id24, id28, id32;
    xtfloatx2 id5, id6, id7, id13, id14, id15, id21, id22, id23;

  if(kernel_width_pad == 12)
  {
    for(itr_oh=0; itr_oh<out_height; itr_oh++)
    {
        ptr_out = (xtfloatx2 *)p_scratch;
        for(itr_ow=0; itr_ow<((total_out_width+3)>>2); itr_ow++)
        {
            accu_x2_0 = CONST_S(0);
            accu_x2_1 = CONST_S(0);
            accu_x2_0_a = CONST_S(0);
            accu_x2_1_a = CONST_S(0);
            accu_x2_0_b = CONST_S(0);
            accu_x2_1_b = CONST_S(0);
            accu_x2_0_c = CONST_S(0);
            accu_x2_1_c = CONST_S(0);

            ptr_ker = (xtfloatx4 *)p_ker;
            ptr_inp = (xtfloatx4 *)p_inp;
            AE_ADDCIRC16X4_XC((ae_int16x4 *)ptr_inp, ((itr_oh*y_stride)*input_width+4*itr_ow)*sizeof(FLOAT32));
#pragma loop_count min=1
#pragma no_unroll
            for(itr_kh=0; itr_kh<kernel_height; itr_kh++)
            {

                //Input loads
                AE_LSX2X2_XC(id4,id8,(xtfloatx4 *)ptr_inp,16);
                AE_LSX2X2_XC(id12,id16,(xtfloatx4 *)ptr_inp,16);
                AE_LSX2X2_XC(id20,id24,(xtfloatx4 *)ptr_inp,16);
                AE_LSX2X2_XC(id28,id32,(xtfloatx4 *)ptr_inp,sizeof(FLOAT32)*(input_width - 12));

                //Kernel Loads
                AE_LSX2X2_I(ker2,ker3, ptr_ker,16);
                AE_LSX2X2_I(ker4,ker5, ptr_ker,32);
                AE_LSX2X2_IP(ker0,ker1, ptr_ker,3*sizeof(xtfloatx4));

                id5 = XT_SEL32_HL_SX2(id8, id4);
                id6 = XT_SEL32_HL_SX2(id12, id8);
                id13 = id7 = XT_SEL32_HL_SX2(id16, id12);
                id14 = XT_SEL32_HL_SX2(id20, id16);
                id21 = id15 = XT_SEL32_HL_SX2(id24, id20);
                id22 = XT_SEL32_HL_SX2(id28, id24);
                id23 = XT_SEL32_HL_SX2(id32, id28);

                MADDMUX_SX2X2(accu_x2_0,accu_x2_1,ker0,ker0,id4,id8,0);
                MADDMUX_SX2X2(accu_x2_0_a,accu_x2_1_a,ker0,ker0,id5,id6,5);
                MADDMUX_SX2X2(accu_x2_0_b,accu_x2_1_b,ker1,ker1,id8,id12,0);
                MADDMUX_SX2X2(accu_x2_0_c,accu_x2_1_c,ker1,ker1,id6,id7,5);

                MADDMUX_SX2X2(accu_x2_0,accu_x2_1,ker2,ker2,id12,id16,0);
                MADDMUX_SX2X2(accu_x2_0_a,accu_x2_1_a,ker2,ker2,id13,id14,5);
                MADDMUX_SX2X2(accu_x2_0_b,accu_x2_1_b,ker3,ker3,id16,id20,0);
                MADDMUX_SX2X2(accu_x2_0_c,accu_x2_1_c,ker3,ker3,id14,id15,5);

                MADDMUX_SX2X2(accu_x2_0,accu_x2_1,ker4,ker4,id20,id24,0);
                MADDMUX_SX2X2(accu_x2_0_a,accu_x2_1_a,ker4,ker4,id21,id22,5);
                MADDMUX_SX2X2(accu_x2_0_b,accu_x2_1_b,ker5,ker5,id24,id28,0);
                MADDMUX_SX2X2(accu_x2_0_c,accu_x2_1_c,ker5,ker5,id22,id23,5);

            }
            accu_x2_0 += accu_x2_0_a;
            accu_x2_0_b += accu_x2_0_c;
            accu_x2_1 += accu_x2_1_a;
            accu_x2_1_b += accu_x2_1_c;
            accu_x2_0 += accu_x2_0_b;
            accu_x2_1 += accu_x2_1_b;

            *ptr_out++ = accu_x2_0;
            *ptr_out++ = accu_x2_1;
        }

        float *ptr_out1 = (float *)p_scratch;
        for(itr_ow = 0; itr_ow < out_width; itr_ow++)
        {
            p_out[itr_oh*out_width*out_stride+itr_ow*out_stride] = ptr_out1[itr_ow*x_stride] + p_bias[0];
        }
    }
  }
  else if(kernel_width_pad == 8)
  {
    for(itr_oh=0; itr_oh<out_height; itr_oh++)
    {
        ptr_out = (xtfloatx2 *)p_scratch;
        for(itr_ow=0; itr_ow<((total_out_width+3)>>2); itr_ow++)
        {
            accu_x2_0 = CONST_S(0);
            accu_x2_1 = CONST_S(0);
            accu_x2_0_a = CONST_S(0);
            accu_x2_1_a = CONST_S(0);
            accu_x2_0_b = CONST_S(0);
            accu_x2_1_b = CONST_S(0);
            accu_x2_0_c = CONST_S(0);
            accu_x2_1_c = CONST_S(0);

            ptr_ker = (xtfloatx4 *)p_ker;
            ptr_inp = (xtfloatx4 *)p_inp;
            AE_ADDCIRC16X4_XC((ae_int16x4 *)ptr_inp, ((itr_oh*y_stride)*input_width+4*itr_ow)*sizeof(FLOAT32));
#pragma loop_count min=1
#pragma no_unroll
            for(itr_kh=0; itr_kh<kernel_height; itr_kh++)
            {

                //Input loads
                AE_LSX2X2_XC(id4,id8,(xtfloatx4 *)ptr_inp,16);
                AE_LSX2X2_XC(id12,id16,(xtfloatx4 *)ptr_inp,16);
                AE_LSX2X2_XC(id20,id24,(xtfloatx4 *)ptr_inp,sizeof(FLOAT32)*(input_width - 8));

                //Kernel Loads
                AE_LSX2X2_I(ker2,ker3, ptr_ker,16);
                AE_LSX2X2_IP(ker0,ker1, ptr_ker,2*sizeof(xtfloatx4));

                id5 = XT_SEL32_HL_SX2(id8, id4);
                id6 = XT_SEL32_HL_SX2(id12, id8);
                id13 = id7 = XT_SEL32_HL_SX2(id16, id12);
                id14 = XT_SEL32_HL_SX2(id20, id16);
                id15 = XT_SEL32_HL_SX2(id24, id20);

                MADDMUX_SX2X2(accu_x2_0,accu_x2_1,ker0,ker0,id4,id8,0);
                MADDMUX_SX2X2(accu_x2_0_a,accu_x2_1_a,ker0,ker0,id5,id6,5);
                MADDMUX_SX2X2(accu_x2_0_b,accu_x2_1_b,ker1,ker1,id8,id12,0);
                MADDMUX_SX2X2(accu_x2_0_c,accu_x2_1_c,ker1,ker1,id6,id7,5);

                MADDMUX_SX2X2(accu_x2_0,accu_x2_1,ker2,ker2,id12,id16,0);
                MADDMUX_SX2X2(accu_x2_0_a,accu_x2_1_a,ker2,ker2,id13,id14,5);
                MADDMUX_SX2X2(accu_x2_0_b,accu_x2_1_b,ker3,ker3,id16,id20,0);
                MADDMUX_SX2X2(accu_x2_0_c,accu_x2_1_c,ker3,ker3,id14,id15,5);

            }
            accu_x2_0 += accu_x2_0_a;
            accu_x2_0_b += accu_x2_0_c;
            accu_x2_1 += accu_x2_1_a;
            accu_x2_1_b += accu_x2_1_c;
            accu_x2_0 += accu_x2_0_b;
            accu_x2_1 += accu_x2_1_b;

            *ptr_out++ = accu_x2_0;
            *ptr_out++ = accu_x2_1;
        }

        float *ptr_out1 = (float *)p_scratch;
        for(itr_ow = 0; itr_ow < out_width; itr_ow++)
        {
            p_out[itr_oh*out_width*out_stride+itr_ow*out_stride] = ptr_out1[itr_ow*x_stride] + p_bias[0];
        }
    }
  }
  else
  {
    /* No reminder loop, run extra iteration, extra output will be thrown away
    when we pick correct outputs using x_stride */
    for(itr_oh=0; itr_oh<out_height; itr_oh++)
    {
        ptr_out = (xtfloatx2 *)p_scratch;
        for(itr_ow=0; itr_ow<((total_out_width+3)>>2); itr_ow++)
        {
            accu_x2_0 = CONST_S(0);
            accu_x2_1 = CONST_S(0);
            accu_x2_0_a = CONST_S(0);
            accu_x2_1_a = CONST_S(0);
            accu_x2_0_b = CONST_S(0);
            accu_x2_1_b = CONST_S(0);
            accu_x2_0_c = CONST_S(0);
            accu_x2_1_c = CONST_S(0);

            ptr_ker = (xtfloatx4 *)p_ker;
#pragma loop_count min=1
            for(itr_kh=0; itr_kh<kernel_height; itr_kh++)
            {
                ptr_inp = (xtfloatx4 *)p_inp;
                AE_ADDCIRC16X4_XC((ae_int16x4 *)ptr_inp, ((itr_kh+itr_oh*y_stride)*input_width+4*itr_ow)*sizeof(FLOAT32));
#pragma loop_count min=1
#pragma no_unroll

                AE_LSX2X2_XC(id4,id8,(xtfloatx4 *)ptr_inp,16);
                for(itr_kw=0; itr_kw<(kernel_width_pad>>2); itr_kw++)
                {
                    AE_LSX2X2_IP(ker0,ker1, ptr_ker,sizeof(xtfloatx4));
                  //  AE_LSX2XC(id4,ptr_inp,8);
                  //  AE_LSX2XC(id8,ptr_inp,8);
                  //  AE_LSX2XC(id12,ptr_inp,8);
                  //  AE_LSX2XC(id16,ptr_inp,-8);
                    AE_LSX2X2_XC(id12,id16,(xtfloatx4 *)ptr_inp,16);
                    id5 = XT_SEL32_HL_SX2(id8, id4);
                    id6 = XT_SEL32_HL_SX2(id12, id8);
                    id7 = XT_SEL32_HL_SX2(id16, id12);
                    MADDMUX_SX2X2(accu_x2_0,accu_x2_1,ker0,ker0,id4,id8,0);
                    MADDMUX_SX2X2(accu_x2_0_a,accu_x2_1_a,ker0,ker0,id5,id6,5);
                    MADDMUX_SX2X2(accu_x2_0_b,accu_x2_1_b,ker1,ker1,id8,id12,0);
                    MADDMUX_SX2X2(accu_x2_0_c,accu_x2_1_c,ker1,ker1,id6,id7,5);
                    id4=id12;
                    id8=id16;

                }
            }
            accu_x2_0 += accu_x2_0_a;
            accu_x2_0_b += accu_x2_0_c;
            accu_x2_1 += accu_x2_1_a;
            accu_x2_1_b += accu_x2_1_c;
            accu_x2_0 += accu_x2_0_b;
            accu_x2_1 += accu_x2_1_b;

            *ptr_out++ = accu_x2_0;
            *ptr_out++ = accu_x2_1;
        }

        float *ptr_out1 = (float *)p_scratch;
        for(itr_ow = 0; itr_ow < out_width; itr_ow++)
        {
            p_out[itr_oh*out_width*out_stride+itr_ow*out_stride] = ptr_out1[itr_ow*x_stride] + p_bias[0];
        }
    }
  }
}

#define COPY_KERNEL_TO_SCRATCH(p_out, p_in, kh, kw, kw_pad) \
{ \
  int itr_kh, itr_kw; \
  for(itr_kh = 0; itr_kh < kh; itr_kh++) \
  { \
    xtfloatx2 *pae_in = (xtfloatx2 *)(&p_in[itr_kh * kw]); \
    xtfloatx2 *pae_out = (xtfloatx2 *)(&p_out[itr_kh * kw_pad]); \
    xtfloatx2 d_tmp; \
    ae_valign in_a = XT_LASX2PP(pae_in); \
_Pragma("no_unroll") \
    for(itr_kw = 0; itr_kw < (kw >> 1); itr_kw++) \
    { \
      XT_LASX2IP(d_tmp, in_a, pae_in); \
      AE_SSX2IP(d_tmp, pae_out, 8); \
    } \
    if(kw & 1) \
    { \
      *(xtfloat *)pae_out = *(xtfloat *)pae_in; \
    } \
  } \
}

static void xa_nn_conv2d_depthwise_nchw_f32(
    FLOAT32* __restrict__ p_out,
    const FLOAT32* __restrict__ p_kernel,
    const FLOAT32* __restrict__ p_inp,
    const FLOAT32* __restrict__ p_bias,
    WORD32  input_height,
    WORD32  input_width,
    WORD32  input_channels,
    WORD32  kernel_height,
    WORD32  kernel_width,
    WORD32  channels_multiplier,
    WORD32  x_stride,
    WORD32  y_stride,
    WORD32  x_padding,
    WORD32  y_padding,
    WORD32  out_height,
    WORD32  out_width,
    WORD32  out_data_format,
    pVOID p_scratch)
{
    FLOAT32 pad_val = 0.0f;
    xa_nn_conv2d_depthwise_init(
            p_scratch,
            input_height,
            input_width,
            input_channels,
            kernel_height,
            kernel_width,
            channels_multiplier,
            x_stride,
            y_stride,
            x_padding,
            y_padding,
            out_height,
            out_width,
            -1,
            1,
            (pVOID)(&pad_val));

    xa_nn_conv2d_dw_state_t *p_state = (xa_nn_conv2d_dw_state_t *)p_scratch;
    xa_nn_circ_buf_t *p_circ_buf = &(p_state->circ_buf);
    int itr_ic, itr_cm, itr_oh;
    int circ_out_height = (p_circ_buf->rows - kernel_height)/y_stride + 1;
    int kernel_height_pad = ALIGNED_SIZE(kernel_height, 2);
    int kernel_width_pad = ALIGNED_SIZE(kernel_width, 4);
    int rows_to_add, top_pad, bottom_pad, rows_added;
    int input_row;
    const FLOAT32 *pt_inp, *pt_ker;
    FLOAT32 *p_inp_circ;
    int i;
    FLOAT32 *p_kernel_padded = (FLOAT32 *)(p_state->p_scratch);
    p_kernel_padded = (FLOAT32 *)ALIGN_PTR(p_kernel_padded, 16);
    FLOAT32 *p_tmp_out = (FLOAT32 *)(p_kernel_padded + kernel_height_pad * kernel_width_pad);
    p_tmp_out = (FLOAT32 *)ALIGN_PTR(p_tmp_out, 16);

    AE_SETCBEGIN0(p_circ_buf->p_begin);
    AE_SETCEND0(p_circ_buf->p_end);

    /* Initialize whole scratch for padded kernel to padding value, after this
     we only have to copy actual kernel values, padding area should remain
     untouched */
    xtfloatx2 *pae_ker_pad = (xtfloatx2 *)p_kernel_padded;
    for(i = 0; i < ((kernel_height_pad * kernel_width_pad) >> 1); i++)
    {
        pae_ker_pad[i] = XT_CONST_S(0);
    }

    for(itr_ic = 0; itr_ic < input_channels; itr_ic++)
    {
        pt_inp = &p_inp[itr_ic*input_height*input_width];
        for(itr_cm = 0; itr_cm < channels_multiplier; itr_cm++)
        {
            pt_ker = &p_kernel[(itr_ic*channels_multiplier+itr_cm)*kernel_height*kernel_width];
            COPY_KERNEL_TO_SCRATCH(p_kernel_padded, pt_ker, kernel_height, kernel_width, kernel_width_pad);

            CIRC_BUF_ADD_ROWS_INIT(rows_added
                                   ,rows_to_add
                                   ,top_pad
                                   ,bottom_pad
                                   ,input_row
                                   ,input_height
                                   ,input_width
                                   ,kernel_height
                                   ,y_stride
                                   ,x_padding
                                   ,y_padding
                                   ,p_circ_buf
                                   ,pt_inp
                                   )
            for(itr_oh = 0; itr_oh < out_height - (circ_out_height - 1); itr_oh += circ_out_height)
            {
                CIRC_BUF_ADD_ROWS(rows_added
                              ,rows_to_add
                              ,top_pad
                              ,bottom_pad
                              ,input_row
                              ,input_height
                              ,input_width
                              ,circ_out_height
                              ,y_stride
                              ,x_padding
                              ,y_padding
                              ,p_circ_buf
                              ,pt_inp
                              )
                p_inp_circ = (FLOAT32 *)p_circ_buf->p_curr;
                convolve_f32(&p_out[(itr_ic*channels_multiplier+itr_cm)+itr_oh*out_width*(input_channels*channels_multiplier)]
                            ,p_kernel_padded
                            ,p_inp_circ
                            ,&p_bias[itr_ic*channels_multiplier+itr_cm]
                            ,p_circ_buf->row_offset
                            ,kernel_height
                            ,kernel_width
                            ,x_stride
                            ,y_stride
                            ,circ_out_height
                            ,out_width
                            ,input_channels*channels_multiplier
                            ,p_tmp_out
                            );
            }
            CIRC_BUF_ADD_ROWS(rows_added
                              ,rows_to_add
                              ,top_pad
                              ,bottom_pad
                              ,input_row
                              ,input_height
                              ,input_width
                              ,circ_out_height
                              ,y_stride
                              ,x_padding
                              ,y_padding
                              ,p_circ_buf
                              ,pt_inp
                              )
            p_inp_circ = (FLOAT32 *)p_circ_buf->p_curr;
            convolve_f32(&p_out[(itr_ic*channels_multiplier+itr_cm)+itr_oh*out_width*(input_channels*channels_multiplier)]
                        ,p_kernel_padded
                        ,p_inp_circ
                        ,&p_bias[itr_ic*channels_multiplier+itr_cm]
                        ,p_circ_buf->row_offset
                        ,kernel_height
                        ,kernel_width
                        ,x_stride
                        ,y_stride
                        ,(out_height-itr_oh)
                        ,out_width
                        ,input_channels*channels_multiplier
                        ,p_tmp_out
                        );
        }
    }
}

/* 2D Convolution implementation for nhwc input */
static inline void conv2d_nhwc_f32(
        FLOAT32 *__restrict__ p_out,
        const FLOAT32 *__restrict__ p_ker,
        const FLOAT32 *__restrict__ p_inp,
        const FLOAT32 *p_bias,
        int kernel_height,
        int kernel_width,
        int out_height,
        int out_width,
        int out_channels,
        int x_stride,
        int y_stride)
{
    WORD32 ker_channels_pad, inp_channels_pad;
    WORD32 i, itr_oh, itr_ch, itr_kh, itr_kw;
    xtfloatx2 *pt_inp0, *pt_inp1, *pt_ker;
    FLOAT32 *out_ptr0, *out_ptr1;
    xtfloatx2 d_inp0, d_inp1, d_ker0;
    xtfloatx2 d_inp2, d_inp3, d_ker1;
    const xtfloatx2 *pt_bias;
    ae_valign ker_a;
    ae_valign bias_a;
    xtfloatx2 d_acc0, d_acc1, d_bias0;
    xtfloatx2 d_acc2, d_acc3, d_bias1;

    ker_channels_pad = out_channels;
    inp_channels_pad = (out_channels + 1)&(~1);

    for(itr_oh = 0; itr_oh < (out_height-1); itr_oh+=2)
    {
        out_ptr0 = (FLOAT32 *)(&p_out[itr_oh*out_channels*out_width]);
        out_ptr1 = (FLOAT32 *)(&p_out[(itr_oh+1)*out_channels*out_width]);
        pt_bias = (const xtfloatx2 *)p_bias;
        bias_a = XT_LASX2PP(pt_bias);
        for(itr_ch = 0; itr_ch < out_channels; itr_ch+=4)
        {
            pt_inp0 = (xtfloatx2 *)p_inp;
            pt_inp1 = (xtfloatx2 *)p_inp;
            AE_ADDCIRC16X4_XC((ae_int16x4 *)pt_inp0, (itr_ch + itr_oh*y_stride*kernel_width*inp_channels_pad)*sizeof(FLOAT32));
            AE_ADDCIRC16X4_XC((ae_int16x4 *)pt_inp1, (itr_ch + (itr_oh+1)*y_stride*kernel_width*inp_channels_pad)*sizeof(FLOAT32));
            pt_ker = (xtfloatx2 *)(&p_ker[itr_ch]);
            ker_a = XT_LASX2PP(pt_ker);
            d_acc0 = XT_CONST_S(0);
            d_acc1 = XT_CONST_S(0);
            d_acc2 = XT_CONST_S(0);
            d_acc3 = XT_CONST_S(0);
            for(itr_kh = 0; itr_kh < kernel_height; itr_kh++)
            {
                xtfloatx2 *ptt_inp0, *ptt_inp1;
                ptt_inp0 = pt_inp0;
                ptt_inp1 = pt_inp1;
                AE_ADDCIRC16X4_XC((ae_int16x4 *)ptt_inp0, itr_kh*kernel_width*inp_channels_pad*sizeof(FLOAT32));
                AE_ADDCIRC16X4_XC((ae_int16x4 *)ptt_inp1, itr_kh*kernel_width*inp_channels_pad*sizeof(FLOAT32));
                for(itr_kw = 0; itr_kw < kernel_width; itr_kw++)
                {
                    XT_LSX2XC(d_inp0, ptt_inp0, 8);
                    XT_LSX2XC(d_inp1, ptt_inp1, 8);
                    XT_LASX2IP(d_ker0, ker_a, pt_ker);
                    XT_LSX2XC(d_inp2, ptt_inp0, (inp_channels_pad-2)*sizeof(FLOAT32));
                    XT_LSX2XC(d_inp3, ptt_inp1, (inp_channels_pad-2)*sizeof(FLOAT32));
                    XT_LASX2IP(d_ker1, ker_a, pt_ker);
                    pt_ker = (xtfloatx2 *)((FLOAT32 *)pt_ker + (ker_channels_pad-4));
                    ker_a = XT_LASX2PP(pt_ker);
                    XT_MADD_SX2(d_acc0, d_inp0, d_ker0);
                    XT_MADD_SX2(d_acc1, d_inp1, d_ker0);
                    XT_MADD_SX2(d_acc2, d_inp2, d_ker1);
                    XT_MADD_SX2(d_acc3, d_inp3, d_ker1);
                }
            }
            XT_LASX2IP(d_bias0, bias_a, pt_bias);
            d_acc0 = XT_ADD_SX2(d_acc0, d_bias0);
            d_acc1 = XT_ADD_SX2(d_acc1, d_bias0);
            XT_LASX2IP(d_bias1, bias_a, pt_bias);
            d_acc2 = XT_ADD_SX2(d_acc2, d_bias1);
            d_acc3 = XT_ADD_SX2(d_acc3, d_bias1);

#pragma no_unroll
            for(i = 0; i < XT_MIN(out_channels-itr_ch, 4); i++)
            {
                out_ptr0[itr_ch+i] = XT_HIGH_S(d_acc0);
                d_acc0 = XT_SEL32_LH_SX2(d_acc0, d_acc2);
                d_acc2 = XT_SEL32_LH_SX2(d_acc2, d_acc2);
                out_ptr1[itr_ch+i] = XT_HIGH_S(d_acc1);
                d_acc1 = XT_SEL32_LH_SX2(d_acc1, d_acc3);
                d_acc3 = XT_SEL32_LH_SX2(d_acc3, d_acc3);
            }
        }
    }
    if(itr_oh < out_height)
    {
        out_ptr0 = (FLOAT32 *)(&p_out[itr_oh*out_channels*out_width]);
        pt_bias = (const xtfloatx2 *)p_bias;
        bias_a = AE_LA64_PP(pt_bias);
        for(itr_ch = 0; itr_ch < out_channels; itr_ch+=2)
        {
            pt_inp0 = (xtfloatx2 *)p_inp;
            AE_ADDCIRC16X4_XC((ae_int16x4 *)pt_inp0, (itr_ch + itr_oh*y_stride*kernel_width*inp_channels_pad)*sizeof(FLOAT32));
            pt_ker = (xtfloatx2 *)(&p_ker[itr_ch]);
            ker_a = XT_LASX2PP(pt_ker);
            d_acc0 = XT_CONST_S(0);
            for(itr_kh = 0; itr_kh < kernel_height; itr_kh++)
            {
                xtfloatx2 *ptt_inp0;
                ptt_inp0 = pt_inp0;
                AE_ADDCIRC16X4_XC((ae_int16x4 *)ptt_inp0, itr_kh*kernel_width*inp_channels_pad*sizeof(FLOAT32));
#pragma no_unroll
                for(itr_kw = 0; itr_kw < kernel_width; itr_kw++)
                {
                    XT_LSX2XC(d_inp0, ptt_inp0, inp_channels_pad*sizeof(FLOAT32));
                    XT_LASX2IP(d_ker0, ker_a, pt_ker);
                    pt_ker = (xtfloatx2 *)((FLOAT32 *)pt_ker + (ker_channels_pad-2));
                    ker_a = XT_LASX2PP(pt_ker);
                    XT_MADD_SX2(d_acc0, d_inp0, d_ker0);
                }
            }
            XT_LASX2IP(d_bias0, bias_a, pt_bias);
            d_acc0 = XT_ADD_SX2(d_acc0, d_bias0);

#pragma no_unroll
            for(i = 0; i < XT_MIN(out_channels-itr_ch, 2); i++)
            {
                out_ptr0[itr_ch+i] = XT_HIGH_S(d_acc0);
                d_acc0 = XT_SEL32_LH_SX2(d_acc0, d_acc0);
            }
        }
    }
}

static void xa_nn_conv2d_depthwise_nhwc_f32(
        FLOAT32 *__restrict__ p_out,
        const FLOAT32 *__restrict__ p_kernel,
        const FLOAT32 *__restrict__ p_inp,
        const FLOAT32 *__restrict__ p_bias,
        WORD32  input_height,
        WORD32  input_width,
        WORD32  input_channels,
        WORD32  kernel_height,
        WORD32  kernel_width,
        WORD32  channels_multiplier,
        WORD32  x_stride,
        WORD32  y_stride,
        WORD32  x_padding,
        WORD32  y_padding,
        WORD32  out_height,
        WORD32  out_width,
        WORD32  out_data_format,
        pVOID p_scratch)
{
    FLOAT32 pad_val = 0.0f;
    xa_nn_conv2d_depthwise_init(
            p_scratch,
            input_height,
            input_width,
            input_channels,
            kernel_height,
            kernel_width,
            channels_multiplier,
            x_stride,
            y_stride,
            x_padding,
            y_padding,
            out_height,
            out_width,
            -1,
            0
            ,(pVOID)(&pad_val));

    xa_nn_circ_buf_t *p_state = (xa_nn_circ_buf_t *)p_scratch;
    xa_nn_circ_buf_t *p_circ_buf = p_state;
    int itr_ow;
    int cols_to_add, left_pad, right_pad, cols_added;
    int input_col;
    const FLOAT32 *pt_inp;
    FLOAT32 *p_inp_circ;

    AE_SETCBEGIN0(p_circ_buf->p_begin);
    AE_SETCEND0(p_circ_buf->p_end);

    pt_inp = (const FLOAT32 *)p_inp;

    CIRC_BUF_ADD_COLS_INIT(
            cols_added,
            cols_to_add,
            left_pad,
            right_pad,
            input_col,
            input_height,
            input_width,
            input_channels,
            kernel_width,
            channels_multiplier,
            x_stride,
            x_padding,
            y_padding,
            out_height,
            p_circ_buf,
            pt_inp);

    for(itr_ow = 0; itr_ow < out_width; itr_ow++)
    {
        CIRC_BUF_ADD_COLS(
                cols_added,
                cols_to_add,
                left_pad,
                right_pad,
                input_col,
                input_height,
                input_width,
                input_channels,
                kernel_width,
                channels_multiplier,
                x_stride,
                x_padding,
                y_padding,
                out_height,
                p_circ_buf,
                pt_inp);

        p_inp_circ = (FLOAT32 *)p_circ_buf->p_curr;

        conv2d_nhwc_f32(
                (FLOAT32 *)(&p_out[itr_ow*input_channels*channels_multiplier]),
                p_kernel,
                p_inp_circ,
                p_bias,
                kernel_height,
                kernel_width,
                out_height,
                out_width,
                (input_channels * channels_multiplier),
                x_stride,
                y_stride);
    }
}

WORD32 xa_nn_conv2d_depthwise_f32(
        FLOAT32* __restrict__ p_out,
        const FLOAT32* __restrict__ p_kernel,
        const FLOAT32* __restrict__ p_inp,
        const FLOAT32* __restrict__ p_bias,
        WORD32  input_height,
        WORD32  input_width,
        WORD32  input_channels,
        WORD32  kernel_height,
        WORD32  kernel_width,
        WORD32  channels_multiplier,
        WORD32  x_stride,
        WORD32  y_stride,
        WORD32  x_padding,
        WORD32  y_padding,
        WORD32  out_height,
        WORD32  out_width,
        WORD32  inp_data_format,
        WORD32  out_data_format,
        pVOID p_scratch)
{
    /* NULL pointer checks */
    XA_NNLIB_ARG_CHK_PTR(p_out, -1);
    XA_NNLIB_ARG_CHK_PTR(p_kernel, -1);
    XA_NNLIB_ARG_CHK_PTR(p_inp, -1);
    XA_NNLIB_ARG_CHK_PTR(p_bias, -1);
    XA_NNLIB_ARG_CHK_PTR(p_scratch, -1);
    /* Pointer alignment checks */
    XA_NNLIB_ARG_CHK_ALIGN(p_out, sizeof(FLOAT32), -1);
    XA_NNLIB_ARG_CHK_ALIGN(p_kernel, sizeof(FLOAT32), -1);
    XA_NNLIB_ARG_CHK_ALIGN(p_inp, sizeof(FLOAT32), -1);
    XA_NNLIB_ARG_CHK_ALIGN(p_bias, sizeof(FLOAT32), -1);
    XA_NNLIB_ARG_CHK_ALIGN(p_scratch, ALIGNMENT, -1);
    /* Basic Parameter checks */
    XA_NNLIB_ARG_CHK_COND((input_height <= 0 || input_width <= 0), -1);
    XA_NNLIB_ARG_CHK_COND((input_channels <= 0), -1);
    XA_NNLIB_ARG_CHK_COND((kernel_height <= 0 || kernel_width <= 0), -1);
    XA_NNLIB_ARG_CHK_COND((kernel_height > input_height), -1);
    XA_NNLIB_ARG_CHK_COND((kernel_width > input_width), -1);
    XA_NNLIB_ARG_CHK_COND((channels_multiplier <= 0), -1);
    XA_NNLIB_ARG_CHK_COND((y_stride <= 0 || x_stride <= 0), -1);
    XA_NNLIB_ARG_CHK_COND((y_padding < 0 || x_padding < 0), -1);
    XA_NNLIB_ARG_CHK_COND((out_height <= 0 || out_width <= 0), -1);
    XA_NNLIB_ARG_CHK_COND((inp_data_format != 0 && inp_data_format != 1), -1);
    XA_NNLIB_ARG_CHK_COND((out_data_format != 0), -1);
    /* Implementation dependent checks */
    XA_NNLIB_ARG_CHK_COND((y_stride > kernel_height), -1);
    XA_NNLIB_ARG_CHK_COND((x_stride > kernel_width), -1);

    if(inp_data_format == 0)
    {
        xa_nn_conv2d_depthwise_nhwc_f32(
                p_out,
                p_kernel,
                p_inp,
                p_bias,
                input_height,
                input_width,
                input_channels,
                kernel_height,
                kernel_width,
                channels_multiplier,
                x_stride,
                y_stride,
                x_padding,
                y_padding,
                out_height,
                out_width,
                out_data_format,
                p_scratch);
    }
    else if(inp_data_format == 1)
    {
        xa_nn_conv2d_depthwise_nchw_f32(
                p_out,
                p_kernel,
                p_inp,
                p_bias,
                input_height,
                input_width,
                input_channels,
                kernel_height,
                kernel_width,
                channels_multiplier,
                x_stride,
                y_stride,
                x_padding,
                y_padding,
                out_height,
                out_width,
                out_data_format,
                p_scratch);
    }
    return 0;
}
#endif /* #if !HAVE_VFPU */

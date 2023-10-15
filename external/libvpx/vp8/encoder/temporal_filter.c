/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "onyxc_int.h"
#include "onyx_int.h"
#include "systemdependent.h"
#include "quantize.h"
#include "alloccommon.h"
#include "mcomp.h"
#include "firstpass.h"
#include "psnr.h"
#include "vpx_scale/vpxscale.h"
#include "extend.h"
#include "ratectrl.h"
#include "quant_common.h"
#include "segmentation.h"
#include "g_common.h"
#include "vpx_scale/yv12extend.h"
#include "postproc.h"
#include "vpx_mem/vpx_mem.h"
#include "swapyv12buffer.h"
#include "threading.h"
#include "vpx_ports/vpx_timer.h"
#include "vpxerrors.h"

#include <math.h>
#include <limits.h>

#define ALT_REF_MC_ENABLED 1    // dis/enable MC in AltRef filtering
#define ALT_REF_SUBPEL_ENABLED 1 // dis/enable subpel in MC AltRef filtering

#define USE_FILTER_LUT 1
#if VP8_TEMPORAL_ALT_REF

#if USE_FILTER_LUT
static int modifier_lut[7][19] =
{
    // Strength=0
    {16, 13, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // Strength=1
    {16, 15, 10, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // Strength=2
    {16, 15, 13, 9, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // Strength=3
    {16, 16, 15, 13, 10, 7, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // Strength=4
    {16, 16, 15, 14, 13, 11, 9, 7, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // Strength=5
    {16, 16, 16, 15, 15, 14, 13, 11, 10, 8, 7, 5, 3, 0, 0, 0, 0, 0, 0},
    // Strength=6
    {16, 16, 16, 16, 15, 15, 14, 14, 13, 12, 11, 10, 9, 8, 7, 5, 4, 2, 1}
};
#endif
static void build_predictors_mb
(
    MACROBLOCKD *x,
    unsigned char *y_mb_ptr,
    unsigned char *u_mb_ptr,
    unsigned char *v_mb_ptr,
    int stride,
    int mv_row,
    int mv_col,
    unsigned char *pred
)
{
    int offset;
    unsigned char *yptr, *uptr, *vptr;

    // Y
    yptr = y_mb_ptr + (mv_row >> 3) * stride + (mv_col >> 3);

    if ((mv_row | mv_col) & 7)
    {
//        vp8_sixtap_predict16x16_c(yptr, stride,
//                                    mv_col & 7, mv_row & 7, &pred[0], 16);
        x->subpixel_predict16x16(yptr, stride,
                                    mv_col & 7, mv_row & 7, &pred[0], 16);
    }
    else
    {
        //vp8_copy_mem16x16_c (yptr, stride, &pred[0], 16);
        RECON_INVOKE(&x->rtcd->recon, copy16x16)(yptr, stride, &pred[0], 16);
    }

    // U & V
    mv_row >>= 1;
    mv_col >>= 1;
    stride >>= 1;
    offset = (mv_row >> 3) * stride + (mv_col >> 3);
    uptr = u_mb_ptr + offset;
    vptr = v_mb_ptr + offset;

    if ((mv_row | mv_col) & 7)
    {
        x->subpixel_predict8x8(uptr, stride,
                            mv_col & 7, mv_row & 7, &pred[256], 8);
        x->subpixel_predict8x8(vptr, stride,
                            mv_col & 7, mv_row & 7, &pred[320], 8);
    }
    else
    {
        RECON_INVOKE(&x->rtcd->recon, copy8x8)(uptr, stride, &pred[256], 8);
        RECON_INVOKE(&x->rtcd->recon, copy8x8)(vptr, stride, &pred[320], 8);
    }
}
static void apply_temporal_filter
(
    unsigned char *frame1,
    unsigned int stride,
    unsigned char *frame2,
    unsigned int block_size,
    int strength,
    int filter_weight,
    unsigned int *accumulator,
    unsigned int *count
)
{
    int i, j, k;
    int modifier;
    int byte = 0;

#if USE_FILTER_LUT
    int *lut = modifier_lut[strength];
#endif

    for (i = 0,k = 0; i < block_size; i++)
    {
        for (j = 0; j < block_size; j++, k++)
        {

            int src_byte = frame1[byte];
            int pixel_value = *frame2++;

#if USE_FILTER_LUT
            // LUT implementation --
            // improves precision of filter
            modifier = abs(src_byte-pixel_value);
            modifier = modifier>18 ? 0 : lut[modifier];
#else
            modifier   = src_byte;
            modifier  -= pixel_value;
            modifier  *= modifier;
            modifier >>= strength;
            modifier  *= 3;

            if (modifier > 16)
                modifier = 16;

            modifier = 16 - modifier;
#endif
            modifier *= filter_weight;

            count[k] += modifier;
            accumulator[k] += modifier * pixel_value;

            byte++;
        }

        byte += stride - block_size;
    }
}

#if ALT_REF_MC_ENABLED
static int dummy_cost[2*mv_max+1];

static int find_matching_mb
(
    VP8_COMP *cpi,
    YV12_BUFFER_CONFIG *arf_frame,
    YV12_BUFFER_CONFIG *frame_ptr,
    int mb_offset,
    int error_thresh
)
{
    MACROBLOCK *x = &cpi->mb;
    int thissme;
    int step_param;
    int further_steps;
    int n = 0;
    int sadpb = x->sadperbit16;
    int bestsme = INT_MAX;
    int num00 = 0;

    BLOCK *b = &x->block[0];
    BLOCKD *d = &x->e_mbd.block[0];
    MV best_ref_mv1 = {0,0};

    int *mvcost[2]    = { &dummy_cost[mv_max+1], &dummy_cost[mv_max+1] };
    int *mvsadcost[2] = { &dummy_cost[mv_max+1], &dummy_cost[mv_max+1] };

    // Save input state
    unsigned char **base_src = b->base_src;
    int src = b->src;
    int src_stride = b->src_stride;
    unsigned char **base_pre = d->base_pre;
    int pre = d->pre;
    int pre_stride = d->pre_stride;

    // Setup frame pointers
    b->base_src = &arf_frame->y_buffer;
    b->src_stride = arf_frame->y_stride;
    b->src = mb_offset;

    d->base_pre = &frame_ptr->y_buffer;
    d->pre_stride = frame_ptr->y_stride;
    d->pre = mb_offset;

    // Further step/diamond searches as necessary
    if (cpi->Speed < 8)
    {
        step_param = cpi->sf.first_step +
                    ((cpi->Speed > 5) ? 1 : 0);
        further_steps =
            (cpi->sf.max_step_search_steps - 1)-step_param;
    }
    else
    {
        step_param = cpi->sf.first_step + 2;
        further_steps = 0;
    }

    if (1/*cpi->sf.search_method == HEX*/)
    {
        // TODO Check that the 16x16 vf & sdf are selected here
        bestsme = vp8_hex_search(x, b, d,
            &best_ref_mv1, &d->bmi.mv.as_mv,
            step_param,
            sadpb/*x->errorperbit*/,
            &num00, &cpi->fn_ptr[BLOCK_16X16],
            mvsadcost, mvcost);
    }
    else
    {
        int mv_x, mv_y;

        bestsme = cpi->diamond_search_sad(x, b, d,
            &best_ref_mv1, &d->bmi.mv.as_mv,
            step_param,
            sadpb / 2/*x->errorperbit*/,
            &num00, &cpi->fn_ptr[BLOCK_16X16],
            mvsadcost, mvcost); //sadpb < 9

        // Further step/diamond searches as necessary
        n = 0;
        //further_steps = (cpi->sf.max_step_search_steps - 1) - step_param;

        n = num00;
        num00 = 0;

        while (n < further_steps)
        {
            n++;

            if (num00)
                num00--;
            else
            {
                thissme = cpi->diamond_search_sad(x, b, d,
                    &best_ref_mv1, &d->bmi.mv.as_mv,
                    step_param + n,
                    sadpb / 4/*x->errorperbit*/,
                    &num00, &cpi->fn_ptr[BLOCK_16X16],
                    mvsadcost, mvcost); //sadpb = 9

                if (thissme < bestsme)
                {
                    bestsme = thissme;
                    mv_y = d->bmi.mv.as_mv.row;
                    mv_x = d->bmi.mv.as_mv.col;
                }
                else
                {
                    d->bmi.mv.as_mv.row = mv_y;
                    d->bmi.mv.as_mv.col = mv_x;
                }
            }
        }
    }

#if ALT_REF_SUBPEL_ENABLED
    // Try sub-pixel MC?
    //if (bestsme > error_thresh && bestsme < INT_MAX)
    {
        bestsme = cpi->find_fractional_mv_step(x, b, d,
                    &d->bmi.mv.as_mv, &best_ref_mv1,
                    x->errorperbit, &cpi->fn_ptr[BLOCK_16X16],
                    cpi->mb.mvcost);
    }
#endif

    // Save input state
    b->base_src = base_src;
    b->src = src;
    b->src_stride = src_stride;
    d->base_pre = base_pre;
    d->pre = pre;
    d->pre_stride = pre_stride;

    return bestsme;
}
#endif

static void vp8cx_temp_blur1_c
(
    VP8_COMP *cpi,
    int frame_count,
    int alt_ref_index,
    int strength
)
{
    int byte;
    int frame;
    int mb_col, mb_row;
    unsigned int filter_weight[MAX_LAG_BUFFERS];
    unsigned char *mm_ptr = cpi->fp_motion_map;
    int cols = cpi->common.mb_cols;
    int rows = cpi->common.mb_rows;
    int MBs  = cpi->common.MBs;
    int mb_y_offset = 0;
    int mb_uv_offset = 0;
    unsigned int accumulator[384];
    unsigned int count[384];
    MACROBLOCKD *mbd = &cpi->mb.e_mbd;
    YV12_BUFFER_CONFIG *f = cpi->frames[alt_ref_index];
    unsigned char *dst1, *dst2;
    DECLARE_ALIGNED(16, unsigned char,  predictor[384]);

    // Save input state
    unsigned char *y_buffer = mbd->pre.y_buffer;
    unsigned char *u_buffer = mbd->pre.u_buffer;
    unsigned char *v_buffer = mbd->pre.v_buffer;

    if (!cpi->use_weighted_temporal_filter)
    {
        // Temporal filtering is unweighted
        for (frame = 0; frame < frame_count; frame++)
            filter_weight[frame] = 1;
    }

    for (mb_row = 0; mb_row < rows; mb_row++)
    {
#if ALT_REF_MC_ENABLED
        // Reduced search extent by 3 for 6-tap filter & smaller UMV border
        cpi->mb.mv_row_min = -((mb_row * 16) + (VP8BORDERINPIXELS - 19));
        cpi->mb.mv_row_max = ((cpi->common.mb_rows - 1 - mb_row) * 16)
                                + (VP8BORDERINPIXELS - 19);
#endif

        for (mb_col = 0; mb_col < cols; mb_col++)
        {
            int i, j, k, w;
            int weight_cap;
            int stride;

            vpx_memset(accumulator, 0, 384*sizeof(unsigned int));
            vpx_memset(count, 0, 384*sizeof(unsigned int));

#if ALT_REF_MC_ENABLED
            // Reduced search extent by 3 for 6-tap filter & smaller UMV border
            cpi->mb.mv_col_min = -((mb_col * 16) + (VP8BORDERINPIXELS - 19));
            cpi->mb.mv_col_max = ((cpi->common.mb_cols - 1 - mb_col) * 16)
                                    + (VP8BORDERINPIXELS - 19);
#endif

            // Read & process macroblock weights from motion map
            if (cpi->use_weighted_temporal_filter)
            {
                weight_cap = 2;

                for (frame = alt_ref_index-1; frame >= 0; frame--)
                {
                    w = *(mm_ptr + (frame+1)*MBs);
                    filter_weight[frame] = w < weight_cap ? w : weight_cap;
                    weight_cap = w;
                }

                filter_weight[alt_ref_index] = 2;

                weight_cap = 2;

                for (frame = alt_ref_index+1; frame < frame_count; frame++)
                {
                    w = *(mm_ptr + frame*MBs);
                    filter_weight[frame] = w < weight_cap ? w : weight_cap;
                    weight_cap = w;
                }

            }

            for (frame = 0; frame < frame_count; frame++)
            {
                int err;

                if (cpi->frames[frame] == NULL)
                    continue;

                mbd->block[0].bmi.mv.as_mv.row = 0;
                mbd->block[0].bmi.mv.as_mv.col = 0;

#if ALT_REF_MC_ENABLED
                //if (filter_weight[frame] == 0)
                {
#define THRESH_LOW   10000
#define THRESH_HIGH  20000

                    // Correlation has been lost try MC
                    err = find_matching_mb ( cpi,
                                             cpi->frames[alt_ref_index],
                                             cpi->frames[frame],
                                             mb_y_offset,
                                             THRESH_LOW );

                    if (filter_weight[frame] < 2)
                    {
                        // Set weight depending on error
                        filter_weight[frame] = err<THRESH_LOW
                                                ? 2 : err<THRESH_HIGH ? 1 : 0;
                    }
                }
#endif
                if (filter_weight[frame] != 0)
                {
                    // Construct the predictors
                    build_predictors_mb (
                              mbd,
                              cpi->frames[frame]->y_buffer + mb_y_offset,
                              cpi->frames[frame]->u_buffer + mb_uv_offset,
                              cpi->frames[frame]->v_buffer + mb_uv_offset,
                              cpi->frames[frame]->y_stride,
                              mbd->block[0].bmi.mv.as_mv.row,
                              mbd->block[0].bmi.mv.as_mv.col,
                              predictor );

                    // Apply the filter (YUV)
                    apply_temporal_filter ( f->y_buffer + mb_y_offset,
                                            f->y_stride,
                                            predictor,
                                            16,
                                            strength,
                                            filter_weight[frame],
                                            accumulator,
                                            count );

                    apply_temporal_filter ( f->u_buffer + mb_uv_offset,
                                            f->uv_stride,
                                            predictor + 256,
                                            8,
                                            strength,
                                            filter_weight[frame],
                                            accumulator + 256,
                                            count + 256 );

                    apply_temporal_filter ( f->v_buffer + mb_uv_offset,
                                            f->uv_stride,
                                            predictor + 320,
                                            8,
                                            strength,
                                            filter_weight[frame],
                                            accumulator + 320,
                                            count + 320 );
                }
            }

            // Normalize filter output to produce AltRef frame
            dst1 = cpi->alt_ref_buffer.source_buffer.y_buffer;
            stride = cpi->alt_ref_buffer.source_buffer.y_stride;
            byte = mb_y_offset;
            for (i = 0,k = 0; i < 16; i++)
            {
                for (j = 0; j < 16; j++, k++)
                {
                    unsigned int pval = accumulator[k] + (count[k] >> 1);
                    pval *= cpi->fixed_divide[count[k]];
                    pval >>= 19;

                    dst1[byte] = (unsigned char)pval;

                    // move to next pixel
                    byte++;
                }

                byte += stride - 16;
            }

            dst1 = cpi->alt_ref_buffer.source_buffer.u_buffer;
            dst2 = cpi->alt_ref_buffer.source_buffer.v_buffer;
            stride = cpi->alt_ref_buffer.source_buffer.uv_stride;
            byte = mb_uv_offset;
            for (i = 0,k = 256; i < 8; i++)
            {
                for (j = 0; j < 8; j++, k++)
                {
                    int m=k+64;

                    // U
                    unsigned int pval = accumulator[k] + (count[k] >> 1);
                    pval *= cpi->fixed_divide[count[k]];
                    pval >>= 19;
                    dst1[byte] = (unsigned char)pval;

                    // V
                    pval = accumulator[m] + (count[m] >> 1);
                    pval *= cpi->fixed_divide[count[m]];
                    pval >>= 19;
                    dst2[byte] = (unsigned char)pval;

                    // move to next pixel
                    byte++;
                }

                byte += stride - 8;
            }

            mm_ptr++;
            mb_y_offset += 16;
            mb_uv_offset += 8;
        }

        mb_y_offset += 16*f->y_stride-f->y_width;
        mb_uv_offset += 8*f->uv_stride-f->uv_width;
    }

    // Restore input state
    mbd->pre.y_buffer = y_buffer;
    mbd->pre.u_buffer = u_buffer;
    mbd->pre.v_buffer = v_buffer;
}

void vp8cx_temp_filter_c
(
    VP8_COMP *cpi
)
{
    int frame = 0;

    int num_frames_backward = 0;
    int num_frames_forward = 0;
    int frames_to_blur_backward = 0;
    int frames_to_blur_forward = 0;
    int frames_to_blur = 0;
    int start_frame = 0;
    unsigned int filtered = 0;

    int strength = cpi->oxcf.arnr_strength;

    int blur_type = cpi->oxcf.arnr_type;

    int max_frames = cpi->active_arnr_frames;

    num_frames_backward = cpi->last_alt_ref_sei - cpi->source_encode_index;

    if (num_frames_backward < 0)
        num_frames_backward += cpi->oxcf.lag_in_frames;

    num_frames_forward = cpi->oxcf.lag_in_frames - (num_frames_backward + 1);

    switch (blur_type)
    {
    case 1:
        /////////////////////////////////////////
        // Backward Blur

        frames_to_blur_backward = num_frames_backward;

        if (frames_to_blur_backward >= max_frames)
            frames_to_blur_backward = max_frames - 1;

        frames_to_blur = frames_to_blur_backward + 1;
        break;

    case 2:
        /////////////////////////////////////////
        // Forward Blur

        frames_to_blur_forward = num_frames_forward;

        if (frames_to_blur_forward >= max_frames)
            frames_to_blur_forward = max_frames - 1;

        frames_to_blur = frames_to_blur_forward + 1;
        break;

    case 3:
    default:
        /////////////////////////////////////////
        // Center Blur
        frames_to_blur_forward = num_frames_forward;
        frames_to_blur_backward = num_frames_backward;

        if (frames_to_blur_forward > frames_to_blur_backward)
            frames_to_blur_forward = frames_to_blur_backward;

        if (frames_to_blur_backward > frames_to_blur_forward)
            frames_to_blur_backward = frames_to_blur_forward;

        // When max_frames is even we have 1 more frame backward than forward
        if (frames_to_blur_forward > (max_frames - 1) / 2)
            frames_to_blur_forward = ((max_frames - 1) / 2);

        if (frames_to_blur_backward > (max_frames / 2))
            frames_to_blur_backward = (max_frames / 2);

        frames_to_blur = frames_to_blur_backward + frames_to_blur_forward + 1;
        break;
    }

    start_frame = (cpi->last_alt_ref_sei
                    + frames_to_blur_forward) % cpi->oxcf.lag_in_frames;

#ifdef DEBUGFWG
    // DEBUG FWG
    printf("max:%d FBCK:%d FFWD:%d ftb:%d ftbbck:%d ftbfwd:%d sei:%d lasei:%d start:%d"
           , max_frames
           , num_frames_backward
           , num_frames_forward
           , frames_to_blur
           , frames_to_blur_backward
           , frames_to_blur_forward
           , cpi->source_encode_index
           , cpi->last_alt_ref_sei
           , start_frame);
#endif

    // Setup frame pointers, NULL indicates frame not included in filter
    vpx_memset(cpi->frames, 0, max_frames*sizeof(YV12_BUFFER_CONFIG *));
    for (frame = 0; frame < frames_to_blur; frame++)
    {
        int which_buffer =  start_frame - frame;

        if (which_buffer < 0)
            which_buffer += cpi->oxcf.lag_in_frames;

        cpi->frames[frames_to_blur-1-frame]
                = &cpi->src_buffer[which_buffer].source_buffer;
    }

    vp8cx_temp_blur1_c (
        cpi,
        frames_to_blur,
        frames_to_blur_backward,
        strength );
}
#endif

/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <assert.h>
#include "pragmas.h"

#include "tokenize.h"
#include "treewriter.h"
#include "onyx_int.h"
#include "modecosts.h"
#include "encodeintra.h"
#include "entropymode.h"
#include "reconinter.h"
#include "reconintra.h"
#include "reconintra4x4.h"
#include "findnearmv.h"
#include "encodemb.h"
#include "quantize.h"
#include "idct.h"
#include "g_common.h"
#include "variance.h"
#include "mcomp.h"

#include "vpx_mem/vpx_mem.h"
#include "dct.h"
#include "systemdependent.h"

#define DIAMONDSEARCH 1
#if CONFIG_RUNTIME_CPU_DETECT
#define IF_RTCD(x)  (x)
#else
#define IF_RTCD(x)  NULL
#endif


void vp8cx_mb_init_quantizer(VP8_COMP *cpi, MACROBLOCK *x);


#define RDFUNC(RM,DM,R,D,target_rd) ( ((128+(R)*(RM)) >> 8) + (DM)*(D) )
/*int  RDFUNC( int RM,int DM, int R, int D, int target_r )
{
    int rd_value;

    rd_value =  ( ((128+(R)*(RM)) >> 8) + (DM)*(D) );

    return rd_value;
}*/

#define UVRDFUNC(RM,DM,R,D,target_r)  RDFUNC(RM,DM,R,D,target_r)

#define RDCOST(RM,DM,R,D) ( ((128+(R)*(RM)) >> 8) + (DM)*(D) )

#define MAXF(a,b)            (((a) > (b)) ? (a) : (b))



const int vp8_auto_speed_thresh[17] =
{
    1000,
    200,
    150,
    130,
    150,
    125,
    120,
    115,
    115,
    115,
    115,
    115,
    115,
    115,
    115,
    115,
    105
};

const MB_PREDICTION_MODE vp8_mode_order[MAX_MODES] =
{
    ZEROMV,
    DC_PRED,

    NEARESTMV,
    NEARMV,

    ZEROMV,
    NEARESTMV,

    ZEROMV,
    NEARESTMV,

    NEARMV,
    NEARMV,

    V_PRED,
    H_PRED,
    TM_PRED,

    NEWMV,
    NEWMV,
    NEWMV,

    SPLITMV,
    SPLITMV,
    SPLITMV,

    B_PRED,
};

const MV_REFERENCE_FRAME vp8_ref_frame_order[MAX_MODES] =
{
    LAST_FRAME,
    INTRA_FRAME,

    LAST_FRAME,
    LAST_FRAME,

    GOLDEN_FRAME,
    GOLDEN_FRAME,

    ALTREF_FRAME,
    ALTREF_FRAME,

    GOLDEN_FRAME,
    ALTREF_FRAME,

    INTRA_FRAME,
    INTRA_FRAME,
    INTRA_FRAME,

    LAST_FRAME,
    GOLDEN_FRAME,
    ALTREF_FRAME,

    LAST_FRAME,
    GOLDEN_FRAME,
    ALTREF_FRAME,

    INTRA_FRAME,
};

static void fill_token_costs(
    unsigned int c      [BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [vp8_coef_tokens],
    const vp8_prob p    [BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [vp8_coef_tokens-1]
)
{
    int i, j, k;


    for (i = 0; i < BLOCK_TYPES; i++)
        for (j = 0; j < COEF_BANDS; j++)
            for (k = 0; k < PREV_COEF_CONTEXTS; k++)

                vp8_cost_tokens((int *)(c [i][j][k]), p [i][j][k], vp8_coef_tree);

}

static int rd_iifactor [ 32 ] =  {    4,   4,   3,   2,   1,   0,   0,   0,
                                      0,   0,   0,   0,   0,   0,   0,   0,
                                      0,   0,   0,   0,   0,   0,   0,   0,
                                      0,   0,   0,   0,   0,   0,   0,   0,
                                 };


// The values in this table should be reviewed
static int sad_per_bit16lut[128] =
{
    4,  4, 4, 4,  4, 4, 4, 4,   // 4
    4,  4, 4, 4,  4, 4, 4, 4,   // 1
    4,  4, 4, 4,  4, 4, 4, 4,   // 2
    4,  4, 4, 4,  4, 4, 4, 4,   // 3
    4,  4, 4, 4,  4, 4, 4, 4,   // 4
    4,  4, 12, 12, 13, 13, 14, 14, // 5
    14, 14, 14, 15, 15, 15, 15, 15, // 6
    15, 15, 15, 15, 15, 15, 15, 15, // 7
    15, 15, 15, 15, 15, 16, 16, 16, // 8
    16, 16, 18, 18, 18, 18, 19, 19, // 9
    19, 19, 19, 19, 19, 19, 19, 19, // 10
    20, 20, 22, 22, 22, 22, 21, 21, // 11
    22, 22, 22, 22, 22, 22, 22, 22, // 12
    22, 22, 22, 22, 22, 22, 22, 22, // 13
    22, 22, 22, 22, 22, 22, 22, 22, // 14
    22, 22, 22, 22, 22, 22, 22, 22, // 15
};

static int sad_per_bit4lut[128] =
{
    4,  4, 4, 4,  4, 4, 4, 4,   // 4
    4,  4, 4, 4,  4, 4, 4, 4,   // 1
    4,  4, 4, 4,  4, 4, 4, 4,   // 2
    4,  4, 4, 4,  4, 4, 4, 4,   // 3
    4,  4, 4, 4,  4, 4, 4, 4,   // 4
    4,  4, 15, 15, 15, 15, 16, 16, // 5
    16, 17, 17, 17, 17, 17, 17, 17, // 6
    17, 17, 19, 19, 22, 22, 21, 21, // 7
    23, 23, 23, 23, 23, 24, 24, 24, // 8
    25, 25, 27, 27, 27, 27, 28, 28, // 9
    28, 28, 29, 29, 29, 29, 29, 29, // 10
    30, 30, 31, 31, 31, 31, 32, 32, // 11
    34, 34, 34, 34, 34, 34, 34, 34, // 12
    34, 34, 34, 34, 34, 34, 34, 34, // 13
    34, 34, 34, 34, 34, 34, 34, 34, // 14
    34, 34, 34, 34, 34, 34, 34, 34, // 15
};

void vp8cx_initialize_me_consts(VP8_COMP *cpi, int QIndex)
{
    cpi->mb.sadperbit16 =  sad_per_bit16lut[QIndex];
    cpi->mb.sadperbit4  =  sad_per_bit4lut[QIndex];
}

void vp8_initialize_rd_consts(VP8_COMP *cpi, int Qvalue)
{
    int q;
    int i;
    int *thresh;
    int threshmult;
    double capped_q = (Qvalue < 160) ? (double)Qvalue : 160.0;
    double rdconst = 3.00;

    vp8_clear_system_state();  //__asm emms;

    // Further tests required to see if optimum is different
    // for key frames, golden frames and arf frames.
    // if (cpi->common.refresh_golden_frame ||
    //     cpi->common.refresh_alt_ref_frame)
    cpi->RDMULT = (int)(rdconst * (capped_q * capped_q));

    // Extend rate multiplier along side quantizer zbin increases
    if (cpi->zbin_over_quant  > 0)
    {
        double oq_factor;
        double modq;

        // Experimental code using the same basic equation as used for Q above
        // The units of cpi->zbin_over_quant are 1/128 of Q bin size
        oq_factor = 1.0 + ((double)0.0015625 * cpi->zbin_over_quant);
        modq = (int)((double)capped_q * oq_factor);
        cpi->RDMULT = (int)(rdconst * (modq * modq));
    }

    if (cpi->pass == 2 && (cpi->common.frame_type != KEY_FRAME))
    {
        if (cpi->next_iiratio > 31)
            cpi->RDMULT += (cpi->RDMULT * rd_iifactor[31]) >> 4;
        else
            cpi->RDMULT += (cpi->RDMULT * rd_iifactor[cpi->next_iiratio]) >> 4;
    }

    if (cpi->RDMULT < 125)
        cpi->RDMULT = 125;

    cpi->mb.errorperbit = (cpi->RDMULT / 100);

    if (cpi->mb.errorperbit < 1)
        cpi->mb.errorperbit = 1;

    vp8_set_speed_features(cpi);

    if (cpi->common.simpler_lpf)
        cpi->common.filter_type = SIMPLE_LOOPFILTER;

    q = (int)pow(Qvalue, 1.25);

    if (q < 8)
        q = 8;

    if (cpi->ref_frame_flags == VP8_ALT_FLAG)
    {
        thresh      = &cpi->rd_threshes[THR_NEWA];
        threshmult  = cpi->sf.thresh_mult[THR_NEWA];
    }
    else if (cpi->ref_frame_flags == VP8_GOLD_FLAG)
    {
        thresh      = &cpi->rd_threshes[THR_NEWG];
        threshmult  = cpi->sf.thresh_mult[THR_NEWG];
    }
    else
    {
        thresh      = &cpi->rd_threshes[THR_NEWMV];
        threshmult  = cpi->sf.thresh_mult[THR_NEWMV];
    }

    if (cpi->RDMULT > 1000)
    {
        cpi->RDDIV = 1;
        cpi->RDMULT /= 100;

        for (i = 0; i < MAX_MODES; i++)
        {
            if (cpi->sf.thresh_mult[i] < INT_MAX)
            {
                cpi->rd_threshes[i] = cpi->sf.thresh_mult[i] * q / 100;
            }
            else
            {
                cpi->rd_threshes[i] = INT_MAX;
            }

            cpi->rd_baseline_thresh[i] = cpi->rd_threshes[i];
        }
    }
    else
    {
        cpi->RDDIV = 100;

        for (i = 0; i < MAX_MODES; i++)
        {
            if (cpi->sf.thresh_mult[i] < (INT_MAX / q))
            {
                cpi->rd_threshes[i] = cpi->sf.thresh_mult[i] * q;
            }
            else
            {
                cpi->rd_threshes[i] = INT_MAX;
            }

            cpi->rd_baseline_thresh[i] = cpi->rd_threshes[i];
        }
    }

    fill_token_costs(
        cpi->mb.token_costs,
        (const vp8_prob( *)[8][3][11]) cpi->common.fc.coef_probs
    );

    vp8_init_mode_costs(cpi);

}

void vp8_auto_select_speed(VP8_COMP *cpi)
{
    int used = cpi->oxcf.cpu_used;

    int milliseconds_for_compress = (int)(1000000 / cpi->oxcf.frame_rate);

    milliseconds_for_compress = milliseconds_for_compress * (16 - cpi->oxcf.cpu_used) / 16;

#if 0

    if (0)
    {
        FILE *f;

        f = fopen("speed.stt", "a");
        fprintf(f, " %8ld %10ld %10ld %10ld\n",
                cpi->common.current_video_frame, cpi->Speed, milliseconds_for_compress, cpi->avg_pick_mode_time);
        fclose(f);
    }

#endif

    /*
    // this is done during parameter valid check
    if( used > 16)
        used = 16;
    if( used < -16)
        used = -16;
    */

    if (cpi->avg_pick_mode_time < milliseconds_for_compress && (cpi->avg_encode_time - cpi->avg_pick_mode_time) < milliseconds_for_compress)
    {
        if (cpi->avg_pick_mode_time == 0)
        {
            cpi->Speed = 4;
        }
        else
        {
            if (milliseconds_for_compress * 100 < cpi->avg_encode_time * 95)
            {
                cpi->Speed          += 2;
                cpi->avg_pick_mode_time = 0;
                cpi->avg_encode_time = 0;

                if (cpi->Speed > 16)
                {
                    cpi->Speed = 16;
                }
            }

            if (milliseconds_for_compress * 100 > cpi->avg_encode_time * vp8_auto_speed_thresh[cpi->Speed])
            {
                cpi->Speed          -= 1;
                cpi->avg_pick_mode_time = 0;
                cpi->avg_encode_time = 0;

                // In real-time mode, cpi->speed is in [4, 16].
                if (cpi->Speed < 4)        //if ( cpi->Speed < 0 )
                {
                    cpi->Speed = 4;        //cpi->Speed = 0;
                }
            }
        }
    }
    else
    {
        cpi->Speed += 4;

        if (cpi->Speed > 16)
            cpi->Speed = 16;


        cpi->avg_pick_mode_time = 0;
        cpi->avg_encode_time = 0;
    }
}

int vp8_block_error_c(short *coeff, short *dqcoeff)
{
    int i;
    int error = 0;

    for (i = 0; i < 16; i++)
    {
        int this_diff = coeff[i] - dqcoeff[i];
        error += this_diff * this_diff;
    }

    return error;
}

int vp8_mbblock_error_c(MACROBLOCK *mb, int dc)
{
    BLOCK  *be;
    BLOCKD *bd;
    int i, j;
    int berror, error = 0;

    for (i = 0; i < 16; i++)
    {
        be = &mb->block[i];
        bd = &mb->e_mbd.block[i];

        berror = 0;

        for (j = dc; j < 16; j++)
        {
            int this_diff = be->coeff[j] - bd->dqcoeff[j];
            berror += this_diff * this_diff;
        }

        error += berror;
    }

    return error;
}

int vp8_mbuverror_c(MACROBLOCK *mb)
{

    BLOCK  *be;
    BLOCKD *bd;


    int i;
    int error = 0;

    for (i = 16; i < 24; i++)
    {
        be = &mb->block[i];
        bd = &mb->e_mbd.block[i];

        error += vp8_block_error_c(be->coeff, bd->dqcoeff);
    }

    return error;
}

#if !(CONFIG_REALTIME_ONLY)
static int macro_block_max_error(MACROBLOCK *mb)
{
    int error = 0;
    int dc = 0;
    BLOCK  *be;
    int i, j;
    int berror;

    dc = !(mb->e_mbd.mode_info_context->mbmi.mode == B_PRED || mb->e_mbd.mode_info_context->mbmi.mode == SPLITMV);

    for (i = 0; i < 16; i++)
    {
        be = &mb->block[i];

        berror = 0;

        for (j = dc; j < 16; j++)
        {
            int this_diff = be->coeff[j];
            berror += this_diff * this_diff;
        }

        error += berror;
    }

    for (i = 16; i < 24; i++)
    {
        be = &mb->block[i];
        berror = 0;

        for (j = 0; j < 16; j++)
        {
            int this_diff = be->coeff[j];
            berror += this_diff * this_diff;
        }

        error += berror;
    }

    error <<= 2;

    if (dc)
    {
        be = &mb->block[24];
        berror = 0;

        for (j = 0; j < 16; j++)
        {
            int this_diff = be->coeff[j];
            berror += this_diff * this_diff;
        }

        error += berror;
    }

    error >>= 4;
    return error;
}
#endif

int VP8_UVSSE(MACROBLOCK *x, const vp8_variance_rtcd_vtable_t *rtcd)
{
    unsigned char *uptr, *vptr;
    unsigned char *upred_ptr = (*(x->block[16].base_src) + x->block[16].src);
    unsigned char *vpred_ptr = (*(x->block[20].base_src) + x->block[20].src);
    int uv_stride = x->block[16].src_stride;

    unsigned int sse1 = 0;
    unsigned int sse2 = 0;
    int mv_row;
    int mv_col;
    int offset;
    int pre_stride = x->e_mbd.block[16].pre_stride;

    vp8_build_uvmvs(&x->e_mbd, 0);
    mv_row = x->e_mbd.block[16].bmi.mv.as_mv.row;
    mv_col = x->e_mbd.block[16].bmi.mv.as_mv.col;

    offset = (mv_row >> 3) * pre_stride + (mv_col >> 3);
    uptr = x->e_mbd.pre.u_buffer + offset;
    vptr = x->e_mbd.pre.v_buffer + offset;

    if ((mv_row | mv_col) & 7)
    {
        VARIANCE_INVOKE(rtcd, subpixvar8x8)(uptr, pre_stride, mv_col & 7, mv_row & 7, upred_ptr, uv_stride, &sse2);
        VARIANCE_INVOKE(rtcd, subpixvar8x8)(vptr, pre_stride, mv_col & 7, mv_row & 7, vpred_ptr, uv_stride, &sse1);
        sse2 += sse1;
    }
    else
    {
        VARIANCE_INVOKE(rtcd, subpixvar8x8)(uptr, pre_stride, mv_col & 7, mv_row & 7, upred_ptr, uv_stride, &sse2);
        VARIANCE_INVOKE(rtcd, subpixvar8x8)(vptr, pre_stride, mv_col & 7, mv_row & 7, vpred_ptr, uv_stride, &sse1);
        sse2 += sse1;
    }

    return sse2;

}

#if !(CONFIG_REALTIME_ONLY)
static int cost_coeffs(MACROBLOCK *mb, BLOCKD *b, int type, ENTROPY_CONTEXT *a, ENTROPY_CONTEXT *l)
{
    int c = !type;              /* start at coef 0, unless Y with Y2 */
    int eob = b->eob;
    int pt ;    /* surrounding block/prev coef predictor */
    int cost = 0;
    short *qcoeff_ptr = b->qcoeff;

    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

# define QC( I)  ( qcoeff_ptr [vp8_default_zig_zag1d[I]] )

    for (; c < eob; c++)
    {
        int v = QC(c);
        int t = vp8_dct_value_tokens_ptr[v].Token;
        cost += mb->token_costs [type] [vp8_coef_bands[c]] [pt] [t];
        cost += vp8_dct_value_cost_ptr[v];
        pt = vp8_prev_token_class[t];
    }

# undef QC

    if (c < 16)
        cost += mb->token_costs [type] [vp8_coef_bands[c]] [pt] [DCT_EOB_TOKEN];

    pt = (c != !type); // is eob first coefficient;
    *a = *l = pt;

    return cost;
}

int vp8_rdcost_mby(MACROBLOCK *mb)
{
    int cost = 0;
    int b;
    int type = 0;
    MACROBLOCKD *x = &mb->e_mbd;
    ENTROPY_CONTEXT_PLANES t_above, t_left;
    ENTROPY_CONTEXT *ta;
    ENTROPY_CONTEXT *tl;

    vpx_memcpy(&t_above, mb->e_mbd.above_context, sizeof(ENTROPY_CONTEXT_PLANES));
    vpx_memcpy(&t_left, mb->e_mbd.left_context, sizeof(ENTROPY_CONTEXT_PLANES));

    ta = (ENTROPY_CONTEXT *)&t_above;
    tl = (ENTROPY_CONTEXT *)&t_left;

    if (x->mode_info_context->mbmi.mode == SPLITMV)
        type = 3;

    for (b = 0; b < 16; b++)
        cost += cost_coeffs(mb, x->block + b, type,
                    ta + vp8_block2above[b], tl + vp8_block2left[b]);

    if (x->mode_info_context->mbmi.mode != SPLITMV)
        cost += cost_coeffs(mb, x->block + 24, 1,
                    ta + vp8_block2above[24], tl + vp8_block2left[24]);

    return cost;
}


static void rd_pick_intra4x4block(
    VP8_COMP *cpi,
    MACROBLOCK *x,
    BLOCK *be,
    BLOCKD *b,
    B_PREDICTION_MODE *best_mode,
    B_PREDICTION_MODE above,
    B_PREDICTION_MODE left,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,

    int *bestrate,
    int *bestratey,
    int *bestdistortion)
{
    B_PREDICTION_MODE mode;
    int best_rd = INT_MAX;       // 1<<30
    int rate = 0;
    int distortion;
    unsigned int *mode_costs;

    ENTROPY_CONTEXT ta = *a, tempa = *a;
    ENTROPY_CONTEXT tl = *l, templ = *l;


    if (x->e_mbd.frame_type == KEY_FRAME)
    {
        mode_costs  = x->bmode_costs[above][left];
    }
    else
    {
        mode_costs = x->inter_bmode_costs;
    }

    for (mode = B_DC_PRED; mode <= B_HU_PRED; mode++)
    {
        int this_rd;
        int ratey;

        rate = mode_costs[mode];
        vp8_encode_intra4x4block_rd(IF_RTCD(&cpi->rtcd), x, be, b, mode);

        tempa = ta;
        templ = tl;

        ratey = cost_coeffs(x, b, 3, &tempa, &templ);
        rate += ratey;
        distortion = ENCODEMB_INVOKE(IF_RTCD(&cpi->rtcd.encodemb), berr)(be->coeff, b->dqcoeff) >> 2;

        this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

        if (this_rd < best_rd)
        {
            *bestrate = rate;
            *bestratey = ratey;
            *bestdistortion = distortion;
            best_rd = this_rd;
            *best_mode = mode;
            *a = tempa;
            *l = templ;
        }
    }

    b->bmi.mode = (B_PREDICTION_MODE)(*best_mode);
    vp8_encode_intra4x4block_rd(IF_RTCD(&cpi->rtcd), x, be, b, b->bmi.mode);

}


int vp8_rd_pick_intra4x4mby_modes(VP8_COMP *cpi, MACROBLOCK *mb, int *Rate, int *rate_y, int *Distortion)
{
    MACROBLOCKD *const xd = &mb->e_mbd;
    int i;
    int cost = mb->mbmode_cost [xd->frame_type] [B_PRED];
    int distortion = 0;
    int tot_rate_y = 0;
    ENTROPY_CONTEXT_PLANES t_above, t_left;
    ENTROPY_CONTEXT *ta;
    ENTROPY_CONTEXT *tl;

    vpx_memcpy(&t_above, mb->e_mbd.above_context, sizeof(ENTROPY_CONTEXT_PLANES));
    vpx_memcpy(&t_left, mb->e_mbd.left_context, sizeof(ENTROPY_CONTEXT_PLANES));

    ta = (ENTROPY_CONTEXT *)&t_above;
    tl = (ENTROPY_CONTEXT *)&t_left;

    vp8_intra_prediction_down_copy(xd);

    for (i = 0; i < 16; i++)
    {
        MODE_INFO *const mic = xd->mode_info_context;
        const int mis = xd->mode_info_stride;
        const B_PREDICTION_MODE A = vp8_above_bmi(mic, i, mis)->mode;
        const B_PREDICTION_MODE L = vp8_left_bmi(mic, i)->mode;
        B_PREDICTION_MODE UNINITIALIZED_IS_SAFE(best_mode);
        int UNINITIALIZED_IS_SAFE(r), UNINITIALIZED_IS_SAFE(ry), UNINITIALIZED_IS_SAFE(d);

        rd_pick_intra4x4block(
            cpi, mb, mb->block + i, xd->block + i, &best_mode, A, L,
            ta + vp8_block2above[i],
            tl + vp8_block2left[i], &r, &ry, &d);

        cost += r;
        distortion += d;
        tot_rate_y += ry;
        mic->bmi[i].mode = xd->block[i].bmi.mode = best_mode;
    }

    *Rate = cost;
    *rate_y += tot_rate_y;
    *Distortion = distortion;

    return RDCOST(mb->rdmult, mb->rddiv, cost, distortion);
}

int vp8_rd_pick_intra16x16mby_mode(VP8_COMP *cpi, MACROBLOCK *x, int *Rate, int *rate_y, int *Distortion)
{

    MB_PREDICTION_MODE mode;
    MB_PREDICTION_MODE UNINITIALIZED_IS_SAFE(mode_selected);
    int rate, ratey;
    unsigned int distortion;
    int best_rd = INT_MAX;

    //Y Search for 16x16 intra prediction mode
    for (mode = DC_PRED; mode <= TM_PRED; mode++)
    {
        int this_rd;
        int dummy;
        rate = 0;

        x->e_mbd.mode_info_context->mbmi.mode = mode;

        rate += x->mbmode_cost[x->e_mbd.frame_type][x->e_mbd.mode_info_context->mbmi.mode];

        vp8_encode_intra16x16mbyrd(IF_RTCD(&cpi->rtcd), x);

        ratey = vp8_rdcost_mby(x);

        rate += ratey;

        VARIANCE_INVOKE(&cpi->rtcd.variance, get16x16var)(x->src.y_buffer, x->src.y_stride, x->e_mbd.dst.y_buffer, x->e_mbd.dst.y_stride, &distortion, &dummy);

        this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

        if (this_rd < best_rd)
        {
            mode_selected = mode;
            best_rd = this_rd;
            *Rate = rate;
            *rate_y = ratey;
            *Distortion = (int)distortion;
        }
    }

    x->e_mbd.mode_info_context->mbmi.mode = mode_selected;
    return best_rd;
}


static int rd_cost_mbuv(MACROBLOCK *mb)
{
    int b;
    int cost = 0;
    MACROBLOCKD *x = &mb->e_mbd;
    ENTROPY_CONTEXT_PLANES t_above, t_left;
    ENTROPY_CONTEXT *ta;
    ENTROPY_CONTEXT *tl;

    vpx_memcpy(&t_above, mb->e_mbd.above_context, sizeof(ENTROPY_CONTEXT_PLANES));
    vpx_memcpy(&t_left, mb->e_mbd.left_context, sizeof(ENTROPY_CONTEXT_PLANES));

    ta = (ENTROPY_CONTEXT *)&t_above;
    tl = (ENTROPY_CONTEXT *)&t_left;

    for (b = 16; b < 20; b++)
        cost += cost_coeffs(mb, x->block + b, vp8_block2type[b],
                    ta + vp8_block2above[b], tl + vp8_block2left[b]);

    for (b = 20; b < 24; b++)
        cost += cost_coeffs(mb, x->block + b, vp8_block2type[b],
                    ta + vp8_block2above[b], tl + vp8_block2left[b]);

    return cost;
}


unsigned int vp8_get_mbuvrecon_error(const vp8_variance_rtcd_vtable_t *rtcd, const MACROBLOCK *x) // sum of squares
{
    unsigned int sse0, sse1;
    int sum0, sum1;
    VARIANCE_INVOKE(rtcd, get8x8var)(x->src.u_buffer, x->src.uv_stride, x->e_mbd.dst.u_buffer, x->e_mbd.dst.uv_stride, &sse0, &sum0);
    VARIANCE_INVOKE(rtcd, get8x8var)(x->src.v_buffer, x->src.uv_stride, x->e_mbd.dst.v_buffer, x->e_mbd.dst.uv_stride, &sse1, &sum1);
    return (sse0 + sse1);
}

static int vp8_rd_inter_uv(VP8_COMP *cpi, MACROBLOCK *x, int *rate, int *distortion, int fullpixel)
{
    vp8_build_uvmvs(&x->e_mbd, fullpixel);
    vp8_encode_inter16x16uvrd(IF_RTCD(&cpi->rtcd), x);


    *rate       = rd_cost_mbuv(x);
    *distortion = ENCODEMB_INVOKE(&cpi->rtcd.encodemb, mbuverr)(x) / 4;

    return UVRDFUNC(x->rdmult, x->rddiv, *rate, *distortion, cpi->target_bits_per_mb);
}

int vp8_rd_pick_intra_mbuv_mode(VP8_COMP *cpi, MACROBLOCK *x, int *rate, int *rate_tokenonly, int *distortion)
{
    MB_PREDICTION_MODE mode;
    MB_PREDICTION_MODE UNINITIALIZED_IS_SAFE(mode_selected);
    int best_rd = INT_MAX;
    int UNINITIALIZED_IS_SAFE(d), UNINITIALIZED_IS_SAFE(r);
    int rate_to;

    for (mode = DC_PRED; mode <= TM_PRED; mode++)
    {
        int rate;
        int distortion;
        int this_rd;

        x->e_mbd.mode_info_context->mbmi.uv_mode = mode;
        vp8_encode_intra16x16mbuvrd(IF_RTCD(&cpi->rtcd), x);

        rate_to = rd_cost_mbuv(x);
        rate = rate_to + x->intra_uv_mode_cost[x->e_mbd.frame_type][x->e_mbd.mode_info_context->mbmi.uv_mode];

        distortion = vp8_get_mbuvrecon_error(IF_RTCD(&cpi->rtcd.variance), x);

        this_rd = UVRDFUNC(x->rdmult, x->rddiv, rate, distortion, cpi->target_bits_per_mb);

        if (this_rd < best_rd)
        {
            best_rd = this_rd;
            d = distortion;
            r = rate;
            *rate_tokenonly = rate_to;
            mode_selected = mode;
        }
    }

    *rate = r;
    *distortion = d;

    x->e_mbd.mode_info_context->mbmi.uv_mode = mode_selected;
    return best_rd;
}
#endif

int vp8_cost_mv_ref(MB_PREDICTION_MODE m, const int near_mv_ref_ct[4])
{
    vp8_prob p [VP8_MVREFS-1];
    assert(NEARESTMV <= m  &&  m <= SPLITMV);
    vp8_mv_ref_probs(p, near_mv_ref_ct);
    return vp8_cost_token(vp8_mv_ref_tree, p,
                          vp8_mv_ref_encoding_array - NEARESTMV + m);
}

void vp8_set_mbmode_and_mvs(MACROBLOCK *x, MB_PREDICTION_MODE mb, MV *mv)
{
    int i;

    x->e_mbd.mode_info_context->mbmi.mode = mb;
    x->e_mbd.mode_info_context->mbmi.mv.as_mv.row = mv->row;
    x->e_mbd.mode_info_context->mbmi.mv.as_mv.col = mv->col;

    for (i = 0; i < 16; i++)
    {
        B_MODE_INFO *bmi = &x->e_mbd.block[i].bmi;
        bmi->mode = (B_PREDICTION_MODE) mb;
        bmi->mv.as_mv.row = mv->row;
        bmi->mv.as_mv.col = mv->col;
    }
}

#if !(CONFIG_REALTIME_ONLY)
static int labels2mode(
    MACROBLOCK *x,
    int const *labelings, int which_label,
    B_PREDICTION_MODE this_mode,
    MV *this_mv, MV *best_ref_mv,
    int *mvcost[2]
)
{
    MACROBLOCKD *const xd = & x->e_mbd;
    MODE_INFO *const mic = xd->mode_info_context;
    const int mis = xd->mode_info_stride;

    int cost = 0;
    int thismvcost = 0;

    /* We have to be careful retrieving previously-encoded motion vectors.
       Ones from this macroblock have to be pulled from the BLOCKD array
       as they have not yet made it to the bmi array in our MB_MODE_INFO. */

    int i = 0;

    do
    {
        BLOCKD *const d = xd->block + i;
        const int row = i >> 2,  col = i & 3;

        B_PREDICTION_MODE m;

        if (labelings[i] != which_label)
            continue;

        if (col  &&  labelings[i] == labelings[i-1])
            m = LEFT4X4;
        else if (row  &&  labelings[i] == labelings[i-4])
            m = ABOVE4X4;
        else
        {
            // the only time we should do costing for new motion vector or mode
            // is when we are on a new label  (jbb May 08, 2007)
            switch (m = this_mode)
            {
            case NEW4X4 :
                thismvcost  = vp8_mv_bit_cost(this_mv, best_ref_mv, mvcost, 102);
                break;
            case LEFT4X4:
                *this_mv = col ? d[-1].bmi.mv.as_mv : vp8_left_bmi(mic, i)->mv.as_mv;
                break;
            case ABOVE4X4:
                *this_mv = row ? d[-4].bmi.mv.as_mv : vp8_above_bmi(mic, i, mis)->mv.as_mv;
                break;
            case ZERO4X4:
                this_mv->row = this_mv->col = 0;
                break;
            default:
                break;
            }

            if (m == ABOVE4X4)  // replace above with left if same
            {
                const MV mv = col ? d[-1].bmi.mv.as_mv : vp8_left_bmi(mic, i)->mv.as_mv;

                if (mv.row == this_mv->row  &&  mv.col == this_mv->col)
                    m = LEFT4X4;
            }

            cost = x->inter_bmode_costs[ m];
        }

        d->bmi.mode = m;
        d->bmi.mv.as_mv = *this_mv;

    }
    while (++i < 16);

    cost += thismvcost ;
    return cost;
}

static int rdcost_mbsegment_y(MACROBLOCK *mb, const int *labels,
                              int which_label, ENTROPY_CONTEXT *ta,
                              ENTROPY_CONTEXT *tl)
{
    int cost = 0;
    int b;
    MACROBLOCKD *x = &mb->e_mbd;

    for (b = 0; b < 16; b++)
        if (labels[ b] == which_label)
            cost += cost_coeffs(mb, x->block + b, 3,
                                ta + vp8_block2above[b],
                                tl + vp8_block2left[b]);

    return cost;

}
static unsigned int vp8_encode_inter_mb_segment(MACROBLOCK *x, int const *labels, int which_label, const vp8_encodemb_rtcd_vtable_t *rtcd)
{
    int i;
    unsigned int distortion = 0;

    for (i = 0; i < 16; i++)
    {
        if (labels[i] == which_label)
        {
            BLOCKD *bd = &x->e_mbd.block[i];
            BLOCK *be = &x->block[i];


            vp8_build_inter_predictors_b(bd, 16, x->e_mbd.subpixel_predict);
            ENCODEMB_INVOKE(rtcd, subb)(be, bd, 16);
            x->vp8_short_fdct4x4(be->src_diff, be->coeff, 32);

            // set to 0 no way to account for 2nd order DC so discount
            //be->coeff[0] = 0;
            x->quantize_b(be, bd);

            distortion += ENCODEMB_INVOKE(rtcd, berr)(be->coeff, bd->dqcoeff);
        }
    }

    return distortion;
}

static void macro_block_yrd(MACROBLOCK *mb, int *Rate, int *Distortion, const vp8_encodemb_rtcd_vtable_t *rtcd)
{
    int b;
    MACROBLOCKD *const x = &mb->e_mbd;
    BLOCK   *const mb_y2 = mb->block + 24;
    BLOCKD *const x_y2  = x->block + 24;
    short *Y2DCPtr = mb_y2->src_diff;
    BLOCK *beptr;
    int d;

    ENCODEMB_INVOKE(rtcd, submby)(mb->src_diff, mb->src.y_buffer, mb->e_mbd.predictor, mb->src.y_stride);

    // Fdct and building the 2nd order block
    for (beptr = mb->block; beptr < mb->block + 16; beptr += 2)
    {
        mb->vp8_short_fdct8x4(beptr->src_diff, beptr->coeff, 32);
        *Y2DCPtr++ = beptr->coeff[0];
        *Y2DCPtr++ = beptr->coeff[16];
    }

    // 2nd order fdct
    if (x->mode_info_context->mbmi.mode != SPLITMV)
    {
        mb->short_walsh4x4(mb_y2->src_diff, mb_y2->coeff, 8);
    }

    // Quantization
    for (b = 0; b < 16; b++)
    {
        mb->quantize_b(&mb->block[b], &mb->e_mbd.block[b]);
    }

    // DC predication and Quantization of 2nd Order block
    if (x->mode_info_context->mbmi.mode != SPLITMV)
    {

        {
            mb->quantize_b(mb_y2, x_y2);
        }
    }

    // Distortion
    if (x->mode_info_context->mbmi.mode == SPLITMV)
        d = ENCODEMB_INVOKE(rtcd, mberr)(mb, 0) << 2;
    else
    {
        d = ENCODEMB_INVOKE(rtcd, mberr)(mb, 1) << 2;
        d += ENCODEMB_INVOKE(rtcd, berr)(mb_y2->coeff, x_y2->dqcoeff);
    }

    *Distortion = (d >> 4);

    // rate
    *Rate = vp8_rdcost_mby(mb);
}

unsigned char vp8_mbsplit_offset2[4][16] = {
    { 0,  8,  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0},
    { 0,  2,  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0},
    { 0,  2,  8, 10,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0},
    { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15}
};
static int vp8_rd_pick_best_mbsegmentation(VP8_COMP *cpi, MACROBLOCK *x, MV *best_ref_mv, int best_rd, int *mdcounts, int *returntotrate, int *returnyrate, int *returndistortion, int compressor_speed, int *mvcost[2], int mvthresh, int fullpixel)
{
    int i, segmentation;
    B_PREDICTION_MODE this_mode;
    MACROBLOCKD *xc = &x->e_mbd;
    BLOCK *c;
    BLOCKD *e;
    int const *labels;
    int best_segment_rd = INT_MAX;
    int best_seg = 0;
    int br = 0;
    int bd = 0;
    int bsr = 0;
    int bsd = 0;
    int bestsegmentyrate = 0;

    static const int segmentation_to_sseshift[4] = {3, 3, 2, 0};

    // FIX TO Rd error outrange bug PGW 9 june 2004
    B_PREDICTION_MODE bmodes[16] = {ZERO4X4, ZERO4X4, ZERO4X4, ZERO4X4,
                                    ZERO4X4, ZERO4X4, ZERO4X4, ZERO4X4,
                                    ZERO4X4, ZERO4X4, ZERO4X4, ZERO4X4,
                                    ZERO4X4, ZERO4X4, ZERO4X4, ZERO4X4
                                   };

    MV bmvs[16];
    int beobs[16];

    vpx_memset(beobs, 0, sizeof(beobs));


    for (segmentation = 0; segmentation < VP8_NUMMBSPLITS; segmentation++)
    {
        int label_count;
        int this_segment_rd = 0;
        int label_mv_thresh;
        int rate = 0;
        int sbr = 0;
        int sbd = 0;
        int sseshift;
        int segmentyrate = 0;

        vp8_variance_fn_ptr_t *v_fn_ptr;

        ENTROPY_CONTEXT_PLANES t_above, t_left;
        ENTROPY_CONTEXT *ta;
        ENTROPY_CONTEXT *tl;
        ENTROPY_CONTEXT_PLANES t_above_b, t_left_b;
        ENTROPY_CONTEXT *ta_b;
        ENTROPY_CONTEXT *tl_b;

        vpx_memcpy(&t_above, x->e_mbd.above_context, sizeof(ENTROPY_CONTEXT_PLANES));
        vpx_memcpy(&t_left, x->e_mbd.left_context, sizeof(ENTROPY_CONTEXT_PLANES));

        ta = (ENTROPY_CONTEXT *)&t_above;
        tl = (ENTROPY_CONTEXT *)&t_left;
        ta_b = (ENTROPY_CONTEXT *)&t_above_b;
        tl_b = (ENTROPY_CONTEXT *)&t_left_b;

        br = 0;
        bd = 0;

        v_fn_ptr = &cpi->fn_ptr[segmentation];
        sseshift = segmentation_to_sseshift[segmentation];
        labels = vp8_mbsplits[segmentation];
        label_count = vp8_mbsplit_count[segmentation];

        // 64 makes this threshold really big effectively
        // making it so that we very rarely check mvs on
        // segments.   setting this to 1 would make mv thresh
        // roughly equal to what it is for macroblocks
        label_mv_thresh = 1 * mvthresh / label_count ;

        // Segmentation method overheads
        rate = vp8_cost_token(vp8_mbsplit_tree, vp8_mbsplit_probs, vp8_mbsplit_encodings + segmentation);

        rate += vp8_cost_mv_ref(SPLITMV, mdcounts);

        this_segment_rd += RDFUNC(x->rdmult, x->rddiv, rate, 0, cpi->target_bits_per_mb);
        br += rate;

        for (i = 0; i < label_count; i++)
        {
            MV mode_mv[B_MODE_COUNT];
            int best_label_rd = INT_MAX;
            B_PREDICTION_MODE mode_selected = ZERO4X4;
            int j;
            int bestlabelyrate = 0;


            // find first label
            j = vp8_mbsplit_offset2[segmentation][i];

            c = &x->block[j];
            e = &x->e_mbd.block[j];

            // search for the best motion vector on this segment
            for (this_mode = LEFT4X4; this_mode <= NEW4X4 ; this_mode ++)
            {
                int distortion;
                int this_rd;
                int num00;
                int labelyrate;
                ENTROPY_CONTEXT_PLANES t_above_s, t_left_s;
                ENTROPY_CONTEXT *ta_s;
                ENTROPY_CONTEXT *tl_s;

                vpx_memcpy(&t_above_s, &t_above, sizeof(ENTROPY_CONTEXT_PLANES));
                vpx_memcpy(&t_left_s, &t_left, sizeof(ENTROPY_CONTEXT_PLANES));

                ta_s = (ENTROPY_CONTEXT *)&t_above_s;
                tl_s = (ENTROPY_CONTEXT *)&t_left_s;

                if (this_mode == NEW4X4)
                {
                    int step_param = 0;
                    int further_steps = (MAX_MVSEARCH_STEPS - 1) - step_param;
                    int n;
                    int thissme;
                    int bestsme = INT_MAX;
                    MV  temp_mv;

                    // Is the best so far sufficiently good that we cant justify doing and new motion search.
                    if (best_label_rd < label_mv_thresh)
                        break;

                    {
                        int sadpb = x->sadperbit4;

                        if (cpi->sf.search_method == HEX)
                            bestsme = vp8_hex_search(x, c, e, best_ref_mv, &mode_mv[NEW4X4], step_param, sadpb/*x->errorperbit*/, &num00, v_fn_ptr, x->mvsadcost, mvcost);
                        else
                        {
                            bestsme = cpi->diamond_search_sad(x, c, e, best_ref_mv, &mode_mv[NEW4X4], step_param, sadpb / 2/*x->errorperbit*/, &num00, v_fn_ptr, x->mvsadcost, mvcost);

                            n = num00;
                            num00 = 0;

                            while (n < further_steps)
                            {
                                n++;

                                if (num00)
                                    num00--;
                                else
                                {
                                    thissme = cpi->diamond_search_sad(x, c, e, best_ref_mv, &temp_mv, step_param + n, sadpb / 2/*x->errorperbit*/, &num00, v_fn_ptr, x->mvsadcost, mvcost);

                                    if (thissme < bestsme)
                                    {
                                        bestsme = thissme;
                                        mode_mv[NEW4X4].row = temp_mv.row;
                                        mode_mv[NEW4X4].col = temp_mv.col;
                                    }
                                }
                            }
                        }

                        // Should we do a full search (best quality only)
                        if ((compressor_speed == 0) && (bestsme >> sseshift) > 4000)
                        {
                            thissme = cpi->full_search_sad(x, c, e, best_ref_mv, sadpb / 4, 16, v_fn_ptr, x->mvcost, x->mvsadcost);

                            if (thissme < bestsme)
                            {
                                bestsme = thissme;
                                mode_mv[NEW4X4] = e->bmi.mv.as_mv;
                            }
                            else
                            {
                                // The full search result is actually worse so re-instate the previous best vector
                                e->bmi.mv.as_mv = mode_mv[NEW4X4];
                            }
                        }
                    }

                    if (bestsme < INT_MAX)
                    {
                        if (!fullpixel)
                            cpi->find_fractional_mv_step(x, c, e, &mode_mv[NEW4X4], best_ref_mv, x->errorperbit / 2, v_fn_ptr, mvcost);
                        else
                            vp8_skip_fractional_mv_step(x, c, e, &mode_mv[NEW4X4], best_ref_mv, x->errorperbit, v_fn_ptr, mvcost);
                    }
                }

                rate = labels2mode(x, labels, i, this_mode, &mode_mv[this_mode], best_ref_mv, mvcost);

                // Trap vectors that reach beyond the UMV borders
                if (((mode_mv[this_mode].row >> 3) < x->mv_row_min) || ((mode_mv[this_mode].row >> 3) > x->mv_row_max) ||
                    ((mode_mv[this_mode].col >> 3) < x->mv_col_min) || ((mode_mv[this_mode].col >> 3) > x->mv_col_max))
                {
                    continue;
                }

                distortion = vp8_encode_inter_mb_segment(x, labels, i, IF_RTCD(&cpi->rtcd.encodemb)) / 4;

                labelyrate = rdcost_mbsegment_y(x, labels, i, ta_s, tl_s);
                rate += labelyrate;

                this_rd = RDFUNC(x->rdmult, x->rddiv, rate, distortion, cpi->target_bits_per_mb);

                if (this_rd < best_label_rd)
                {
                    sbr = rate;
                    sbd = distortion;
                    bestlabelyrate = labelyrate;
                    mode_selected = this_mode;
                    best_label_rd = this_rd;

                    vpx_memcpy(ta_b, ta_s, sizeof(ENTROPY_CONTEXT_PLANES));
                    vpx_memcpy(tl_b, tl_s, sizeof(ENTROPY_CONTEXT_PLANES));

                }
            }

            vpx_memcpy(ta, ta_b, sizeof(ENTROPY_CONTEXT_PLANES));
            vpx_memcpy(tl, tl_b, sizeof(ENTROPY_CONTEXT_PLANES));

            labels2mode(x, labels, i, mode_selected, &mode_mv[mode_selected], best_ref_mv, mvcost);

            br += sbr;
            bd += sbd;
            segmentyrate += bestlabelyrate;
            this_segment_rd += best_label_rd;

            if ((this_segment_rd > best_rd) || (this_segment_rd > best_segment_rd))
                break;
        }

        if ((this_segment_rd <= best_rd) && (this_segment_rd < best_segment_rd))
        {
            bsr = br;
            bsd = bd;
            bestsegmentyrate = segmentyrate;
            best_segment_rd = this_segment_rd;
            best_seg = segmentation;

            // store everything needed to come back to this!!
            for (i = 0; i < 16; i++)
            {
                BLOCKD *bd = &x->e_mbd.block[i];

                bmvs[i] = bd->bmi.mv.as_mv;
                bmodes[i] = bd->bmi.mode;
                beobs[i] = bd->eob;
            }
        }
    }

    // set it to the best
    for (i = 0; i < 16; i++)
    {
        BLOCKD *bd = &x->e_mbd.block[i];

        bd->bmi.mv.as_mv = bmvs[i];
        bd->bmi.mode = bmodes[i];
        bd->eob = beobs[i];
    }

    *returntotrate = bsr;
    *returndistortion = bsd;
    *returnyrate = bestsegmentyrate;

    // save partitions
    labels = vp8_mbsplits[best_seg];
    x->e_mbd.mode_info_context->mbmi.partitioning = best_seg;
    x->partition_info->count = vp8_mbsplit_count[best_seg];

    for (i = 0; i < x->partition_info->count; i++)
    {
        int j;

        j = vp8_mbsplit_offset2[best_seg][i];

        x->partition_info->bmi[i].mode = x->e_mbd.block[j].bmi.mode;
        x->partition_info->bmi[i].mv.as_mv = x->e_mbd.block[j].bmi.mv.as_mv;
    }

    return best_segment_rd;
}


int vp8_rd_pick_inter_mode(VP8_COMP *cpi, MACROBLOCK *x, int recon_yoffset, int recon_uvoffset, int *returnrate, int *returndistortion, int *returnintra)
{
    BLOCK *b = &x->block[0];
    BLOCKD *d = &x->e_mbd.block[0];
    MACROBLOCKD *xd = &x->e_mbd;
    B_MODE_INFO best_bmodes[16];
    MB_MODE_INFO best_mbmode;
    PARTITION_INFO best_partition;
    MV best_ref_mv;
    MV mode_mv[MB_MODE_COUNT];
    MB_PREDICTION_MODE this_mode;
    int num00;
    int best_mode_index = 0;

    int i;
    int mode_index;
    int mdcounts[4];
    int rate;
    int distortion;
    int best_rd = INT_MAX; // 1 << 30;
    int ref_frame_cost[MAX_REF_FRAMES];
    int rate2, distortion2;
    int uv_intra_rate, uv_intra_distortion, uv_intra_rate_tokenonly;
    int rate_y, UNINITIALIZED_IS_SAFE(rate_uv);

    //int all_rds[MAX_MODES];        // Experimental debug code.
    //int all_rates[MAX_MODES];
    //int all_dist[MAX_MODES];
    //int intermodecost[MAX_MODES];

    MB_PREDICTION_MODE uv_intra_mode;
    int sse;
    int sum;
    int uvintra_eob = 0;
    int tteob = 0;
    int force_no_skip = 0;

    *returnintra = INT_MAX;

    vpx_memset(&best_mbmode, 0, sizeof(best_mbmode)); // clean

    cpi->mbs_tested_so_far++;          // Count of the number of MBs tested so far this frame

    x->skip = 0;

    ref_frame_cost[INTRA_FRAME]   = vp8_cost_zero(cpi->prob_intra_coded);

    // Experimental code
    // Adjust the RD multiplier based on the best case distortion we saw in the most recently coded mb
    //if ( (cpi->last_mb_distortion) > 0 && (cpi->target_bits_per_mb > 0) )
    /*{
        int tmprdmult;

        //tmprdmult = (cpi->last_mb_distortion * 256) / ((cpi->av_per_frame_bandwidth*256)/cpi->common.MBs);
        tmprdmult = (cpi->last_mb_distortion * 256) / cpi->target_bits_per_mb;
        //tmprdmult = tmprdmult;

        //if ( tmprdmult > cpi->RDMULT * 2 )
        //  tmprdmult = cpi->RDMULT * 2;
        //else if ( tmprdmult < cpi->RDMULT / 2 )
        //  tmprdmult = cpi->RDMULT / 2;

        //tmprdmult = (tmprdmult < 25) ? 25 : tmprdmult;

        //x->rdmult = tmprdmult;

    }*/

    // Special case treatment when GF and ARF are not sensible options for reference
    if (cpi->ref_frame_flags == VP8_LAST_FLAG)
    {
        ref_frame_cost[LAST_FRAME]    = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_zero(255);
        ref_frame_cost[GOLDEN_FRAME]  = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_one(255)
                                        + vp8_cost_zero(128);
        ref_frame_cost[ALTREF_FRAME]  = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_one(255)
                                        + vp8_cost_one(128);
    }
    else
    {
        ref_frame_cost[LAST_FRAME]    = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_zero(cpi->prob_last_coded);
        ref_frame_cost[GOLDEN_FRAME]  = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_one(cpi->prob_last_coded)
                                        + vp8_cost_zero(cpi->prob_gf_coded);
        ref_frame_cost[ALTREF_FRAME]  = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_one(cpi->prob_last_coded)
                                        + vp8_cost_one(cpi->prob_gf_coded);
    }

    vpx_memset(mode_mv, 0, sizeof(mode_mv));

    x->e_mbd.mode_info_context->mbmi.ref_frame = INTRA_FRAME;
    vp8_rd_pick_intra_mbuv_mode(cpi, x, &uv_intra_rate, &uv_intra_rate_tokenonly, &uv_intra_distortion);
    uv_intra_mode = x->e_mbd.mode_info_context->mbmi.uv_mode;
    {
        uvintra_eob = 0;

        for (i = 16; i < 24; i++)
            uvintra_eob += x->e_mbd.block[i].eob;
    }

    for (mode_index = 0; mode_index < MAX_MODES; mode_index++)
    {
        int frame_cost;
        int this_rd = INT_MAX;
        int lf_or_gf = 0;           // Lat Frame (01) or gf/arf (1)
        int disable_skip = 0;

        force_no_skip = 0;

        // Experimental debug code.
        // Record of rd values recorded for this MB. -1 indicates not measured
        //all_rds[mode_index] = -1;
        //all_rates[mode_index] = -1;
        //all_dist[mode_index] = -1;
        //intermodecost[mode_index] = -1;

        // Test best rd so far against threshold for trying this mode.
        if (best_rd <= cpi->rd_threshes[mode_index])
            continue;



        // These variables hold are rolling total cost and distortion for this mode
        rate2 = 0;
        distortion2 = 0;

        // Where skip is allowable add in the default per mb cost for the no skip case.
        // where we then decide to skip we have to delete this and replace it with the
        // cost of signallying a skip
        if (cpi->common.mb_no_coeff_skip)
        {
            rate2 += vp8_cost_bit(cpi->prob_skip_false, 0);
        }

        this_mode = vp8_mode_order[mode_index];

        x->e_mbd.mode_info_context->mbmi.mode = this_mode;
        x->e_mbd.mode_info_context->mbmi.uv_mode = DC_PRED;
        x->e_mbd.mode_info_context->mbmi.ref_frame = vp8_ref_frame_order[mode_index];

        //Only consider ZEROMV/ALTREF_FRAME for alt ref frame.
        if (cpi->is_src_frame_alt_ref)
        {
            if (this_mode != ZEROMV || x->e_mbd.mode_info_context->mbmi.ref_frame != ALTREF_FRAME)
                continue;
        }

        if (x->e_mbd.mode_info_context->mbmi.ref_frame == LAST_FRAME)
        {
            YV12_BUFFER_CONFIG *lst_yv12 = &cpi->common.yv12_fb[cpi->common.lst_fb_idx];

            if (!(cpi->ref_frame_flags & VP8_LAST_FLAG))
                continue;

            lf_or_gf = 0;  // Local last frame vs Golden frame flag

            // Set up pointers for this macro block into the previous frame recon buffer
            x->e_mbd.pre.y_buffer = lst_yv12->y_buffer + recon_yoffset;
            x->e_mbd.pre.u_buffer = lst_yv12->u_buffer + recon_uvoffset;
            x->e_mbd.pre.v_buffer = lst_yv12->v_buffer + recon_uvoffset;
        }
        else if (x->e_mbd.mode_info_context->mbmi.ref_frame == GOLDEN_FRAME)
        {
            YV12_BUFFER_CONFIG *gld_yv12 = &cpi->common.yv12_fb[cpi->common.gld_fb_idx];

            // not supposed to reference gold frame
            if (!(cpi->ref_frame_flags & VP8_GOLD_FLAG))
                continue;

            lf_or_gf = 1;  // Local last frame vs Golden frame flag

            // Set up pointers for this macro block into the previous frame recon buffer
            x->e_mbd.pre.y_buffer = gld_yv12->y_buffer + recon_yoffset;
            x->e_mbd.pre.u_buffer = gld_yv12->u_buffer + recon_uvoffset;
            x->e_mbd.pre.v_buffer = gld_yv12->v_buffer + recon_uvoffset;
        }
        else if (x->e_mbd.mode_info_context->mbmi.ref_frame == ALTREF_FRAME)
        {
            YV12_BUFFER_CONFIG *alt_yv12 = &cpi->common.yv12_fb[cpi->common.alt_fb_idx];

            // not supposed to reference alt ref frame
            if (!(cpi->ref_frame_flags & VP8_ALT_FLAG))
                continue;

            //if ( !cpi->source_alt_ref_active )
            //  continue;

            lf_or_gf = 1;  // Local last frame vs Golden frame flag

            // Set up pointers for this macro block into the previous frame recon buffer
            x->e_mbd.pre.y_buffer = alt_yv12->y_buffer + recon_yoffset;
            x->e_mbd.pre.u_buffer = alt_yv12->u_buffer + recon_uvoffset;
            x->e_mbd.pre.v_buffer = alt_yv12->v_buffer + recon_uvoffset;
        }

        vp8_find_near_mvs(&x->e_mbd,
                          x->e_mbd.mode_info_context,
                          &mode_mv[NEARESTMV], &mode_mv[NEARMV], &best_ref_mv,
                          mdcounts, x->e_mbd.mode_info_context->mbmi.ref_frame, cpi->common.ref_frame_sign_bias);


        // Estimate the reference frame signaling cost and add it to the rolling cost variable.
        frame_cost = ref_frame_cost[x->e_mbd.mode_info_context->mbmi.ref_frame];
        rate2 += frame_cost;

        if (this_mode <= B_PRED)
        {
            for (i = 0; i < 16; i++)
            {
                vpx_memset(&x->e_mbd.block[i].bmi, 0, sizeof(B_MODE_INFO));
            }
        }

        // Check to see if the testing frequency for this mode is at its max
        // If so then prevent it from being tested and increase the threshold for its testing
        if (cpi->mode_test_hit_counts[mode_index] && (cpi->mode_check_freq[mode_index] > 1))
        {
            if (cpi->mbs_tested_so_far  <= cpi->mode_check_freq[mode_index] * cpi->mode_test_hit_counts[mode_index])
            {
                // Increase the threshold for coding this mode to make it less likely to be chosen
                cpi->rd_thresh_mult[mode_index] += 4;

                if (cpi->rd_thresh_mult[mode_index] > MAX_THRESHMULT)
                    cpi->rd_thresh_mult[mode_index] = MAX_THRESHMULT;

                cpi->rd_threshes[mode_index] = (cpi->rd_baseline_thresh[mode_index] >> 7) * cpi->rd_thresh_mult[mode_index];

                continue;
            }
        }

        // We have now reached the point where we are going to test the current mode so increment the counter for the number of times it has been tested
        cpi->mode_test_hit_counts[mode_index] ++;

        // Experimental code. Special case for gf and arf zeromv modes. Increase zbin size to supress noise
        if (cpi->zbin_mode_boost_enabled)
        {
            if ((vp8_mode_order[mode_index] == ZEROMV) && (vp8_ref_frame_order[mode_index] != LAST_FRAME))
                cpi->zbin_mode_boost = GF_ZEROMV_ZBIN_BOOST;
            else
                cpi->zbin_mode_boost = 0;

            vp8cx_mb_init_quantizer(cpi, x);
        }

        switch (this_mode)
        {
        case B_PRED:

            // Note the rate value returned here includes the cost of coding the BPRED mode : x->mbmode_cost[x->e_mbd.frame_type][BPRED];
            vp8_rd_pick_intra4x4mby_modes(cpi, x, &rate, &rate_y, &distortion);
            rate2 += rate;
            //rate_y = rate;
            distortion2 += distortion;
            rate2 += uv_intra_rate;
            rate_uv = uv_intra_rate_tokenonly;
            distortion2 += uv_intra_distortion;
            break;

        case SPLITMV:
        {
            int frame_cost_rd = RDFUNC(x->rdmult, x->rddiv, frame_cost, 0, cpi->target_bits_per_mb);
            int saved_rate = rate2;

            // vp8_rd_pick_best_mbsegmentation looks only at Y and does not account for frame_cost.
            // (best_rd - frame_cost_rd) is thus a conservative breakout number.
            int breakout_rd = best_rd - frame_cost_rd;
            int tmp_rd;

            if (x->e_mbd.mode_info_context->mbmi.ref_frame == LAST_FRAME)
                tmp_rd = vp8_rd_pick_best_mbsegmentation(cpi, x, &best_ref_mv, breakout_rd, mdcounts, &rate, &rate_y, &distortion, cpi->compressor_speed, x->mvcost, cpi->rd_threshes[THR_NEWMV], cpi->common.full_pixel) ;
            else if (x->e_mbd.mode_info_context->mbmi.ref_frame == GOLDEN_FRAME)
                tmp_rd = vp8_rd_pick_best_mbsegmentation(cpi, x, &best_ref_mv, breakout_rd, mdcounts, &rate, &rate_y, &distortion, cpi->compressor_speed, x->mvcost, cpi->rd_threshes[THR_NEWG], cpi->common.full_pixel) ;
            else
                tmp_rd = vp8_rd_pick_best_mbsegmentation(cpi, x, &best_ref_mv, breakout_rd, mdcounts, &rate, &rate_y, &distortion, cpi->compressor_speed, x->mvcost, cpi->rd_threshes[THR_NEWA], cpi->common.full_pixel) ;

            rate2 += rate;
            distortion2 += distortion;

            // If even the 'Y' rd value of split is higher than best so far then dont bother looking at UV
            if (tmp_rd < breakout_rd)
            {
                // Now work out UV cost and add it in
                vp8_rd_inter_uv(cpi, x, &rate, &distortion, cpi->common.full_pixel);
                rate2 += rate;
                rate_uv = rate;
                distortion2 += distortion;

            }
            else
            {
                this_rd = INT_MAX;
                disable_skip = 1;
            }

            // Trap cases where the best split mode has all vectors coded 0,0 (or all the same)
            if (0)
            {
                int allsame = 1;

                for (i = 1; i < 16; i++)
                {
                    BLOCKD *bd = &x->e_mbd.block[i];

                    if (bd->bmi.mv.as_int != x->e_mbd.block[0].bmi.mv.as_int)   //(bmvs[i].col != bmvs[i-1].col) || (bmvs[i].row != bmvs[i-1].row ) )
                    {
                        allsame = 0;
                        break;
                    }
                }

                if (allsame)
                {
                    // reset mode and mv and jump to newmv
                    this_mode = NEWMV;
                    distortion2 = 0;
                    rate2 = saved_rate;
                    mode_mv[NEWMV].row = x->e_mbd.block[0].bmi.mv.as_mv.row;
                    mode_mv[NEWMV].col = x->e_mbd.block[0].bmi.mv.as_mv.col;
                    rate2 += vp8_mv_bit_cost(&mode_mv[NEWMV], &best_ref_mv, x->mvcost, 96);
                    goto mv_selected;
                }
            }

            // trap cases where the 8x8s can be promoted to 8x16s or 16x8s
            if (0)//x->partition_info->count == 4)
            {

                if (x->partition_info->bmi[0].mv.as_int == x->partition_info->bmi[1].mv.as_int
                    && x->partition_info->bmi[2].mv.as_int == x->partition_info->bmi[3].mv.as_int)
                {
                    const int *labels = vp8_mbsplits[2];
                    x->e_mbd.mode_info_context->mbmi.partitioning = 0;
                    rate -= vp8_cost_token(vp8_mbsplit_tree, vp8_mbsplit_probs, vp8_mbsplit_encodings + 2);
                    rate += vp8_cost_token(vp8_mbsplit_tree, vp8_mbsplit_probs, vp8_mbsplit_encodings);
                    //rate -=  x->inter_bmode_costs[  x->partition_info->bmi[1]];
                    //rate -=  x->inter_bmode_costs[  x->partition_info->bmi[3]];
                    x->partition_info->bmi[1] = x->partition_info->bmi[2];
                }
            }

        }
        break;
        case DC_PRED:
        case V_PRED:
        case H_PRED:
        case TM_PRED:
            x->e_mbd.mode_info_context->mbmi.ref_frame = INTRA_FRAME;
            vp8_build_intra_predictors_mby_ptr(&x->e_mbd);
            {
                macro_block_yrd(x, &rate, &distortion, IF_RTCD(&cpi->rtcd.encodemb)) ;
                rate2 += rate;
                rate_y = rate;
                distortion2 += distortion;
                rate2 += x->mbmode_cost[x->e_mbd.frame_type][x->e_mbd.mode_info_context->mbmi.mode];
                rate2 += uv_intra_rate;
                rate_uv = uv_intra_rate_tokenonly;
                distortion2 += uv_intra_distortion;
            }
            break;

        case NEWMV:

            // Decrement full search counter
            if (cpi->check_freq[lf_or_gf] > 0)
                cpi->check_freq[lf_or_gf] --;

            {
                int thissme;
                int bestsme = INT_MAX;
                int step_param = cpi->sf.first_step;
                int search_range;
                int further_steps;
                int n;

                // Work out how long a search we should do
                search_range = MAXF(abs(best_ref_mv.col), abs(best_ref_mv.row)) >> 3;

                if (search_range >= x->vector_range)
                    x->vector_range = search_range;
                else if (x->vector_range > cpi->sf.min_fs_radius)
                    x->vector_range--;

                // Initial step/diamond search
                {
                    int sadpb = x->sadperbit16;

                    if (cpi->sf.search_method == HEX)
                    {
                        bestsme = vp8_hex_search(x, b, d, &best_ref_mv, &d->bmi.mv.as_mv, step_param, sadpb/*x->errorperbit*/, &num00, &cpi->fn_ptr[BLOCK_16X16], x->mvsadcost, x->mvcost);
                        mode_mv[NEWMV].row = d->bmi.mv.as_mv.row;
                        mode_mv[NEWMV].col = d->bmi.mv.as_mv.col;
                    }
                    else
                    {
                        bestsme = cpi->diamond_search_sad(x, b, d, &best_ref_mv, &d->bmi.mv.as_mv, step_param, sadpb / 2/*x->errorperbit*/, &num00, &cpi->fn_ptr[BLOCK_16X16], x->mvsadcost, x->mvcost); //sadpb < 9
                        mode_mv[NEWMV].row = d->bmi.mv.as_mv.row;
                        mode_mv[NEWMV].col = d->bmi.mv.as_mv.col;

                        // Further step/diamond searches as necessary
                        n = 0;
                        further_steps = (cpi->sf.max_step_search_steps - 1) - step_param;

                        n = num00;
                        num00 = 0;

                        while (n < further_steps)
                        {
                            n++;

                            if (num00)
                                num00--;
                            else
                            {
                                thissme = cpi->diamond_search_sad(x, b, d, &best_ref_mv, &d->bmi.mv.as_mv, step_param + n, sadpb / 4/*x->errorperbit*/, &num00, &cpi->fn_ptr[BLOCK_16X16], x->mvsadcost, x->mvcost); //sadpb = 9

                                if (thissme < bestsme)
                                {
                                    bestsme = thissme;
                                    mode_mv[NEWMV].row = d->bmi.mv.as_mv.row;
                                    mode_mv[NEWMV].col = d->bmi.mv.as_mv.col;
                                }
                                else
                                {
                                    d->bmi.mv.as_mv.row = mode_mv[NEWMV].row;
                                    d->bmi.mv.as_mv.col = mode_mv[NEWMV].col;
                                }
                            }
                        }
                    }

                }

                // Should we do a full search
                if (!cpi->check_freq[lf_or_gf] || cpi->do_full[lf_or_gf])
                {
                    int thissme;
                    int full_flag_thresh = 0;

                    // Update x->vector_range based on best vector found in step search
                    search_range = MAXF(abs(d->bmi.mv.as_mv.row), abs(d->bmi.mv.as_mv.col));

                    if (search_range > x->vector_range)
                        x->vector_range = search_range;
                    else
                        search_range = x->vector_range;

                    // Apply limits
                    search_range = (search_range > cpi->sf.max_fs_radius) ? cpi->sf.max_fs_radius : search_range;
                    {
                        int sadpb = x->sadperbit16 >> 2;
                        thissme = cpi->full_search_sad(x, b, d, &best_ref_mv, sadpb, search_range, &cpi->fn_ptr[BLOCK_16X16], x->mvcost, x->mvsadcost);
                    }

                    // Barrier threshold to initiating full search
                    // full_flag_thresh = 10 + (thissme >> 7);
                    if ((thissme + full_flag_thresh) < bestsme)
                    {
                        cpi->do_full[lf_or_gf] ++;
                        bestsme = thissme;
                    }
                    else if (thissme < bestsme)
                        bestsme = thissme;
                    else
                    {
                        cpi->do_full[lf_or_gf] = cpi->do_full[lf_or_gf] >> 1;
                        cpi->check_freq[lf_or_gf] = cpi->sf.full_freq[lf_or_gf];

                        // The full search result is actually worse so re-instate the previous best vector
                        d->bmi.mv.as_mv.row = mode_mv[NEWMV].row;
                        d->bmi.mv.as_mv.col = mode_mv[NEWMV].col;
                    }
                }

                if (bestsme < INT_MAX)
                    // cpi->find_fractional_mv_step(x,b,d,&d->bmi.mv.as_mv,&best_ref_mv,x->errorperbit/2,cpi->fn_ptr.svf,cpi->fn_ptr.vf,x->mvcost);  // normal mvc=11
                    cpi->find_fractional_mv_step(x, b, d, &d->bmi.mv.as_mv, &best_ref_mv, x->errorperbit / 4, &cpi->fn_ptr[BLOCK_16X16], x->mvcost);

                mode_mv[NEWMV].row = d->bmi.mv.as_mv.row;
                mode_mv[NEWMV].col = d->bmi.mv.as_mv.col;

                // Add the new motion vector cost to our rolling cost variable
                rate2 += vp8_mv_bit_cost(&mode_mv[NEWMV], &best_ref_mv, x->mvcost, 96);

            }

        case NEARESTMV:
        case NEARMV:

            // Clip "next_nearest" so that it does not extend to far out of image
            if (mode_mv[this_mode].col < (xd->mb_to_left_edge - LEFT_TOP_MARGIN))
                mode_mv[this_mode].col = xd->mb_to_left_edge - LEFT_TOP_MARGIN;
            else if (mode_mv[this_mode].col > xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN)
                mode_mv[this_mode].col = xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN;

            if (mode_mv[this_mode].row < (xd->mb_to_top_edge - LEFT_TOP_MARGIN))
                mode_mv[this_mode].row = xd->mb_to_top_edge - LEFT_TOP_MARGIN;
            else if (mode_mv[this_mode].row > xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN)
                mode_mv[this_mode].row = xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN;

            // Do not bother proceeding if the vector (from newmv,nearest or near) is 0,0 as this should then be coded using the zeromv mode.
            if (((this_mode == NEARMV) || (this_mode == NEARESTMV)) &&
                ((mode_mv[this_mode].row == 0) && (mode_mv[this_mode].col == 0)))
                continue;

        case ZEROMV:

        mv_selected:

            // Trap vectors that reach beyond the UMV borders
            // Note that ALL New MV, Nearest MV Near MV and Zero MV code drops through to this point
            // because of the lack of break statements in the previous two cases.
            if (((mode_mv[this_mode].row >> 3) < x->mv_row_min) || ((mode_mv[this_mode].row >> 3) > x->mv_row_max) ||
                ((mode_mv[this_mode].col >> 3) < x->mv_col_min) || ((mode_mv[this_mode].col >> 3) > x->mv_col_max))
                continue;

            vp8_set_mbmode_and_mvs(x, this_mode, &mode_mv[this_mode]);
            vp8_build_inter_predictors_mby(&x->e_mbd);
            VARIANCE_INVOKE(&cpi->rtcd.variance, get16x16var)(x->src.y_buffer, x->src.y_stride, x->e_mbd.predictor, 16, (unsigned int *)(&sse), &sum);

            if (cpi->active_map_enabled && x->active_ptr[0] == 0)
            {
                x->skip = 1;
            }
            else if (sse < x->encode_breakout)
            {
                // Check u and v to make sure skip is ok
                int sse2 = 0;

                sse2 = VP8_UVSSE(x, IF_RTCD(&cpi->rtcd.variance));

                if (sse2 * 2 < x->encode_breakout)
                {
                    x->skip = 1;
                    distortion2 = sse;
                    rate2 = 500;

                    disable_skip = 1;    // We have no real rate data so trying to adjust for rate_y and rate_uv below will cause problems.
                    this_rd = RDFUNC(x->rdmult, x->rddiv, rate2, distortion2, cpi->target_bits_per_mb);

                    break;              // (PGW) Move break here from below - for now at least
                }
                else
                    x->skip = 0;
            }

            //intermodecost[mode_index] = vp8_cost_mv_ref(this_mode, mdcounts);   // Experimental debug code

            // Add in the Mv/mode cost
            rate2 += vp8_cost_mv_ref(this_mode, mdcounts);

            // Y cost and distortion
            macro_block_yrd(x, &rate, &distortion, IF_RTCD(&cpi->rtcd.encodemb));
            rate2 += rate;
            rate_y = rate;
            distortion2 += distortion;

            // UV cost and distortion
            vp8_rd_inter_uv(cpi, x, &rate, &distortion, cpi->common.full_pixel);
            rate2 += rate;
            rate_uv = rate;
            distortion2 += distortion;
            break;

        default:
            break;
        }

        if (!disable_skip)
        {
            // Test for the condition where skip block will be activated because there are no non zero coefficients and make any necessary adjustment for rate
            if (cpi->common.mb_no_coeff_skip)
            {
                tteob = 0;

                for (i = 0; i <= 24; i++)
                {
                    tteob += x->e_mbd.block[i].eob;
                }

                if (tteob == 0)
                {
#if 1
                    rate2 -= (rate_y + rate_uv);

                    // Back out no skip flag costing and add in skip flag costing
                    if (cpi->prob_skip_false)
                    {
                        rate2 += vp8_cost_bit(cpi->prob_skip_false, 1);
                        rate2 -= vp8_cost_bit(cpi->prob_skip_false, 0);
                    }

#else
                    int rateuseskip;
                    int ratenotuseskip;



                    ratenotuseskip = rate_y + rate_uv + vp8_cost_bit(cpi->prob_skip_false, 0);
                    rateuseskip    = vp8_cost_bit(cpi->prob_skip_false, 1);

                    if (1) // rateuseskip<ratenotuseskip)
                    {
                        rate2 -= ratenotuseskip;
                        rate2 += rateuseskip;
                        force_no_skip = 0;
                    }
                    else
                    {
                        force_no_skip = 1;
                    }

#endif
                }

#if             0
                else
                {
                    int rateuseskip;
                    int ratenotuseskip;
                    int maxdistortion;
                    int minrate;
                    int skip_rd;

                    // distortion when no coeff is encoded
                    maxdistortion = macro_block_max_error(x);

                    ratenotuseskip = rate_y + rate_uv + vp8_cost_bit(cpi->prob_skip_false, 0);
                    rateuseskip    = vp8_cost_bit(cpi->prob_skip_false, 1);

                    minrate         = rateuseskip - ratenotuseskip;

                    skip_rd = RDFUNC(x->rdmult, x->rddiv, minrate, maxdistortion - distortion2, cpi->target_bits_per_mb);

                    if (skip_rd + 50 < 0 && x->e_mbd.mbmi.ref_frame != INTRA_FRAME && rate_y + rate_uv < 4000)
                    {
                        force_no_skip = 1;
                        rate2       = rate2 + rateuseskip - ratenotuseskip;
                        distortion2 =  maxdistortion;
                    }
                    else
                    {
                        force_no_skip = 0;
                    }

                }

#endif

            }

            // Calculate the final RD estimate for this mode
            this_rd = RDFUNC(x->rdmult, x->rddiv, rate2, distortion2, cpi->target_bits_per_mb);
        }

        // Experimental debug code.
        //all_rds[mode_index] = this_rd;
        //all_rates[mode_index] = rate2;
        //all_dist[mode_index] = distortion2;

        if ((x->e_mbd.mode_info_context->mbmi.ref_frame == INTRA_FRAME)  && (this_rd < *returnintra))
        {
            *returnintra = this_rd ;
        }

        // Did this mode help.. i.i is it the new best mode
        if (this_rd < best_rd || x->skip)
        {
            // Note index of best mode so far
            best_mode_index = mode_index;
            x->e_mbd.mode_info_context->mbmi.force_no_skip = force_no_skip;

            if (this_mode <= B_PRED)
            {
                x->e_mbd.mode_info_context->mbmi.uv_mode = uv_intra_mode;
            }

            *returnrate = rate2;
            *returndistortion = distortion2;
            best_rd = this_rd;
            vpx_memcpy(&best_mbmode, &x->e_mbd.mode_info_context->mbmi, sizeof(MB_MODE_INFO));
            vpx_memcpy(&best_partition, x->partition_info, sizeof(PARTITION_INFO));

            for (i = 0; i < 16; i++)
            {
                vpx_memcpy(&best_bmodes[i], &x->e_mbd.block[i].bmi, sizeof(B_MODE_INFO));
            }

            // Testing this mode gave rise to an improvement in best error score. Lower threshold a bit for next time
            cpi->rd_thresh_mult[mode_index] = (cpi->rd_thresh_mult[mode_index] >= (MIN_THRESHMULT + 2)) ? cpi->rd_thresh_mult[mode_index] - 2 : MIN_THRESHMULT;
            cpi->rd_threshes[mode_index] = (cpi->rd_baseline_thresh[mode_index] >> 7) * cpi->rd_thresh_mult[mode_index];
        }

        // If the mode did not help improve the best error case then raise the threshold for testing that mode next time around.
        else
        {
            cpi->rd_thresh_mult[mode_index] += 4;

            if (cpi->rd_thresh_mult[mode_index] > MAX_THRESHMULT)
                cpi->rd_thresh_mult[mode_index] = MAX_THRESHMULT;

            cpi->rd_threshes[mode_index] = (cpi->rd_baseline_thresh[mode_index] >> 7) * cpi->rd_thresh_mult[mode_index];
        }

        if (x->skip)
            break;
    }

    // Reduce the activation RD thresholds for the best choice mode
    if ((cpi->rd_baseline_thresh[best_mode_index] > 0) && (cpi->rd_baseline_thresh[best_mode_index] < (INT_MAX >> 2)))
    {
        int best_adjustment = (cpi->rd_thresh_mult[best_mode_index] >> 2);

        cpi->rd_thresh_mult[best_mode_index] = (cpi->rd_thresh_mult[best_mode_index] >= (MIN_THRESHMULT + best_adjustment)) ? cpi->rd_thresh_mult[best_mode_index] - best_adjustment : MIN_THRESHMULT;
        cpi->rd_threshes[best_mode_index] = (cpi->rd_baseline_thresh[best_mode_index] >> 7) * cpi->rd_thresh_mult[best_mode_index];

        // If we chose a split mode then reset the new MV thresholds as well
        /*if ( vp8_mode_order[best_mode_index] == SPLITMV )
        {
            best_adjustment = 4; //(cpi->rd_thresh_mult[THR_NEWMV] >> 4);
            cpi->rd_thresh_mult[THR_NEWMV] = (cpi->rd_thresh_mult[THR_NEWMV] >= (MIN_THRESHMULT+best_adjustment)) ? cpi->rd_thresh_mult[THR_NEWMV]-best_adjustment: MIN_THRESHMULT;
            cpi->rd_threshes[THR_NEWMV] = (cpi->rd_baseline_thresh[THR_NEWMV] >> 7) * cpi->rd_thresh_mult[THR_NEWMV];

            best_adjustment = 4; //(cpi->rd_thresh_mult[THR_NEWG] >> 4);
            cpi->rd_thresh_mult[THR_NEWG] = (cpi->rd_thresh_mult[THR_NEWG] >= (MIN_THRESHMULT+best_adjustment)) ? cpi->rd_thresh_mult[THR_NEWG]-best_adjustment: MIN_THRESHMULT;
            cpi->rd_threshes[THR_NEWG] = (cpi->rd_baseline_thresh[THR_NEWG] >> 7) * cpi->rd_thresh_mult[THR_NEWG];

            best_adjustment = 4; //(cpi->rd_thresh_mult[THR_NEWA] >> 4);
            cpi->rd_thresh_mult[THR_NEWA] = (cpi->rd_thresh_mult[THR_NEWA] >= (MIN_THRESHMULT+best_adjustment)) ? cpi->rd_thresh_mult[THR_NEWA]-best_adjustment: MIN_THRESHMULT;
            cpi->rd_threshes[THR_NEWA] = (cpi->rd_baseline_thresh[THR_NEWA] >> 7) * cpi->rd_thresh_mult[THR_NEWA];
        }*/

    }

    // If we have chosen new mv or split then decay the full search check count more quickly.
    if ((vp8_mode_order[best_mode_index] == NEWMV) || (vp8_mode_order[best_mode_index] == SPLITMV))
    {
        int lf_or_gf = (vp8_ref_frame_order[best_mode_index] == LAST_FRAME) ? 0 : 1;

        if (cpi->check_freq[lf_or_gf] && !cpi->do_full[lf_or_gf])
        {
            cpi->check_freq[lf_or_gf] --;
        }
    }

    // Keep a record of best mode index that we chose
    cpi->last_best_mode_index = best_mode_index;

    // Note how often each mode chosen as best
    cpi->mode_chosen_counts[best_mode_index] ++;


    if (cpi->is_src_frame_alt_ref && (best_mbmode.mode != ZEROMV || best_mbmode.ref_frame != ALTREF_FRAME))
    {
        best_mbmode.mode = ZEROMV;
        best_mbmode.ref_frame = ALTREF_FRAME;
        best_mbmode.mv.as_int = 0;
        best_mbmode.uv_mode = 0;
        best_mbmode.mb_skip_coeff = (cpi->common.mb_no_coeff_skip) ? 1 : 0;
        best_mbmode.partitioning = 0;
        best_mbmode.dc_diff = 0;

        vpx_memcpy(&x->e_mbd.mode_info_context->mbmi, &best_mbmode, sizeof(MB_MODE_INFO));
        vpx_memcpy(x->partition_info, &best_partition, sizeof(PARTITION_INFO));

        for (i = 0; i < 16; i++)
        {
            vpx_memset(&x->e_mbd.block[i].bmi, 0, sizeof(B_MODE_INFO));
        }

        x->e_mbd.mode_info_context->mbmi.mv.as_int = 0;

        return best_rd;
    }


    // macroblock modes
    vpx_memcpy(&x->e_mbd.mode_info_context->mbmi, &best_mbmode, sizeof(MB_MODE_INFO));
    vpx_memcpy(x->partition_info, &best_partition, sizeof(PARTITION_INFO));

    for (i = 0; i < 16; i++)
    {
        vpx_memcpy(&x->e_mbd.block[i].bmi, &best_bmodes[i], sizeof(B_MODE_INFO));
    }

    x->e_mbd.mode_info_context->mbmi.mv.as_mv = x->e_mbd.block[15].bmi.mv.as_mv;

    return best_rd;
}
#endif


/**
 * @file tdl_nec_protocl.c
 * @author www.tuya.com
 * @brief tdl_nec_protocl module is used to 
 * @version 0.1
 * @date 2022-06-06
 *
 * @copyright Copyright (c) tuya.inc 2022
 *
 */

#include "tal_memory.h"
#include "tal_log.h"

#include "tdl_ir_dev_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/

#define ABS(a,b)    ((a)>(b)?(a-b):(b-a))

#define NEC_LEADER_HIGH_US  (9000)
#define NEC_LEADER_LOW_US   (4500)
#define NEC_LOGIC_HIGH_US   (560)  // logic0, l0gic1 high level 
#define NEC_LOGIC0_LOW_US   (560)
#define NEC_LOGIC1_LOW_US   (1690)
#define NEC_REPEAT_HIGH_US  (9000)
#define NEC_REPEAT_LOW_US   (2250)
#define NEC_END_HIGH_US     (560)
#define NEC_CODE_CYCLE      (110000UL)

#define NEC_TIMECODE_LEN_MIN    (68-2)
#define NEC_CODE_DATA_OFFSET    (2)

#define NEC_CODE_DATA_LEN       (64) // 8*4*2

/***********************************************************
***********************typedef define***********************
***********************************************************/

struct nec_err_val_t{
    unsigned int leader_high_err_val;
    unsigned int leader_low_err_val;
    unsigned int logic0_high_err_val;
    unsigned int logic0_low_err_val;
    unsigned int logic1_high_err_val;
    unsigned int logic1_low_err_val;
    unsigned int repeat_high_err_val;
    unsigned int repeat_low_err_val;
    unsigned int end_high_err_val;
};

typedef struct nec_err_val_t NEC_ERR_VAL_T;

/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief nec protocol build
 *
 * @param[in] is_msb: 1: use MSB build, 0: use LSB build
 * @param[in] ir_nec_data: nec protocol data
 * @param[out] build data
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_nec_build(BOOL_T is_msb, IR_DATA_NEC_T ir_nec_data, IR_DATA_TIMECODE_T **timecode)
{
    OPERATE_RET op_ret = OPRT_OK;
    UINT8_T i = 0, j = 0;
    UINT16_T offset = 0;
    UINT16_T len = 0;
    IR_DATA_TIMECODE_T *temp_timecode = NULL;

    UINT8_T nec_data[4];

    len = 2 + 2*8*4 + 2 + 4*ir_nec_data.repeat_cnt;

    temp_timecode = (IR_DATA_TIMECODE_T *)tal_malloc(SIZEOF(IR_DATA_TIMECODE_T) + len * SIZEOF(UINT32_T));
    if (NULL == temp_timecode) {
        return OPRT_MALLOC_FAILED;
    }
    memset(temp_timecode, 0, SIZEOF(IR_DATA_TIMECODE_T) + len*SIZEOF(UINT32_T));

    nec_data[0] = ir_nec_data.addr>>8 & 0x00FF;
    nec_data[1] = ir_nec_data.addr & 0x00FF;
    nec_data[2] = ir_nec_data.cmd>>8 & 0x00FF;
    nec_data[3] = ir_nec_data.cmd & 0x00FF;

    temp_timecode->data = (UINT32_T *)(temp_timecode+1);
    temp_timecode->len = len;

    /* header */
    temp_timecode->data[0] = NEC_LEADER_HIGH_US;
    temp_timecode->data[1] = NEC_LEADER_LOW_US;

    /* addr, cmd  */
    offset = 1;
    for (i=0; i<4; i++) {
        for (j=0; j<8; j++) {
            temp_timecode->data[(i*8+j+offset)*2] = NEC_LOGIC_HIGH_US;
            if (is_msb) {
                if (nec_data[i] & (0x80>>j)) {
                    temp_timecode->data[(i*8+j+offset)*2 + 1] = NEC_LOGIC1_LOW_US;
                } else {
                    temp_timecode->data[(i*8+j+offset)*2 + 1] = NEC_LOGIC0_LOW_US;
                }
            } else {
                if (nec_data[i] & (1<<j)) {
                    temp_timecode->data[(i*8+j+offset)*2 + 1] = NEC_LOGIC1_LOW_US;
                } else {
                    temp_timecode->data[(i*8+j+offset)*2 + 1] = NEC_LOGIC0_LOW_US;
                }
            }
        }
    }

    offset = 33;// 1+16+16
    temp_timecode->data[offset*2] = NEC_END_HIGH_US;
    temp_timecode->data[offset*2 + 1] = NEC_CODE_CYCLE;
    for (i=0; i<(offset*2 + 1); i++) {
        temp_timecode->data[offset*2 + 1] -= temp_timecode->data[i];
    }

    /* repeat */
    offset++;
    for (i=0; i<ir_nec_data.repeat_cnt; i++) {
        temp_timecode->data[offset*2] = NEC_REPEAT_HIGH_US;
        temp_timecode->data[offset*2 + 1] = NEC_REPEAT_LOW_US;
        temp_timecode->data[offset*2 + 2] = NEC_END_HIGH_US;
        temp_timecode->data[offset*2 + 3] = NEC_CODE_CYCLE-NEC_REPEAT_HIGH_US-NEC_REPEAT_LOW_US-NEC_END_HIGH_US;
        offset += 2;
    }

    *timecode = temp_timecode;

    return op_ret;
}

/**
 * @brief nec build data release function
 *
 * @param[in] timecode: parameters in tdl_ir_nec_build function
 *
 * @return none
 */
void tdl_ir_nec_build_release(IR_DATA_TIMECODE_T *timecode)
{
    if (NULL != timecode->data) {
        tal_free(timecode);
        timecode = NULL;
    }

    return;
}

/**
 * @brief nec error value initialization
 *
 * @param[out] nec_err_val: error value
 * @param[in] nec_cfg: nec config parameters
 * 
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_nec_err_val_init(void **nec_err_val, IR_NEC_CFG_T *nec_cfg)
{
    NEC_ERR_VAL_T *err_val = NULL;

    if (NULL == nec_cfg || \
        nec_cfg->lead_err >= 100 || \
        nec_cfg->logics_err >= 100 || \
        nec_cfg->logic0_err >= 100 || \
        nec_cfg->logic1_err >= 100 || \
        nec_cfg->repeat_err >= 100) {
            return OPRT_INVALID_PARM;
    }

    err_val = (NEC_ERR_VAL_T *)tal_malloc(SIZEOF(NEC_ERR_VAL_T));
    if (NULL == nec_err_val) {
        return OPRT_MALLOC_FAILED;
    }

    *nec_err_val = (void *)err_val;

    err_val->leader_high_err_val = NEC_LEADER_HIGH_US*nec_cfg->lead_err/100;
    err_val->leader_low_err_val = NEC_LEADER_LOW_US*nec_cfg->lead_err/100;

    err_val->logic0_high_err_val = NEC_LOGIC_HIGH_US*nec_cfg->logics_err/100;
    err_val->logic0_low_err_val = NEC_LOGIC0_LOW_US*nec_cfg->logic0_err/100;

    err_val->logic1_high_err_val = NEC_LOGIC_HIGH_US*nec_cfg->logics_err/100;
    err_val->logic1_low_err_val = NEC_LOGIC1_LOW_US*nec_cfg->logic1_err/100;

    err_val->repeat_high_err_val = NEC_REPEAT_HIGH_US*nec_cfg->repeat_err/100;
    err_val->repeat_low_err_val = NEC_REPEAT_LOW_US*nec_cfg->repeat_err/100;
    err_val->end_high_err_val = NEC_END_HIGH_US*nec_cfg->repeat_err/100;

    return OPRT_OK;
}

/**
 * @brief ir nec protocol error value initialization release
 *
 * @param[in] nec_err_val: error value
 *
 * @return none
 */
void tdl_ir_nec_err_val_init_release(void *nec_err_val)
{
    if (NULL != nec_err_val) {
        tal_free(nec_err_val);
        nec_err_val = NULL;
    }

    return;
}

/**
 * @brief get nec head
 *
 * @param[in] data: raw data 
 * @param[in] len: data length
 * @param[in] nec_err_val: nec error value
 * @param[in] head_idx: find nec head index
 * 
 * @return nec head, negative number not found
 */
OPERATE_RET tdl_nec_get_frame_head(UINT32_T *data, UINT16_T len, void *nec_err_val, UINT16_T *head_idx)
{
    UINT16_T cur_index = 0;
    NEC_ERR_VAL_T *err_val = NULL;

    err_val = (NEC_ERR_VAL_T *)nec_err_val;

    if (NULL == data || NULL == err_val || NULL == head_idx || len < 2) {
        return OPRT_INVALID_PARM;
    }

    for (cur_index=0; cur_index<len; cur_index++) {
        if ((ABS(data[cur_index], NEC_LEADER_HIGH_US) <= err_val->leader_high_err_val) && \
        (ABS(data[cur_index+1], NEC_LEADER_LOW_US) <= err_val->leader_low_err_val)) {
            (*head_idx) = cur_index;
            return OPRT_OK;
        }
    }

    return OPRT_COM_ERROR;
}

/**
 * @brief nec protocol single parser
 *
 * @param[in] data: raw data 
 * @param[in] len: data length
 * @param[out] nec_code: decode data
 * @param[in] nec_err_val: nec error value
 * @param[in] is_msb: 1: use msb, 0: use lsb
 * 
 * @return parsed data length
 */
UINT16_T tdl_ir_nec_parser_single(unsigned int *data, unsigned int len, IR_DATA_NEC_T *nec_code, void *nec_err_val, unsigned char is_msb)
{
    unsigned int decode_index = 0;
    unsigned int i=0;
    unsigned char nec_data[4] = {0};
    unsigned char data_index, data_cnt;
    NEC_ERR_VAL_T *err_val = NULL;

    err_val = (NEC_ERR_VAL_T *)nec_err_val;

    if (NULL == data || len < NEC_TIMECODE_LEN_MIN || NULL == nec_code) {
        TAL_PR_ERR("nec decode invalid parm, len:%d, mini_len:%d", len, NEC_TIMECODE_LEN_MIN);
        return 0;
    }

    /* output data init */
    nec_code->addr = 0;
    nec_code->cmd = 0;
    nec_code->repeat_cnt = 0;

    /* check leader code */
    if ((ABS(data[decode_index], NEC_LEADER_HIGH_US) > err_val->leader_high_err_val) || \
        (ABS(data[decode_index+1], NEC_LEADER_LOW_US) > err_val->leader_low_err_val)) {
        TAL_PR_ERR("nec single parser, leader code error");
        return 2;
    }

    /* decode data */
    data_cnt = 0;
    data_index = 0;
    decode_index = decode_index + NEC_CODE_DATA_OFFSET;
    for (i=decode_index; i<decode_index+NEC_CODE_DATA_LEN; i=i+2) {
        if ((ABS(data[i], NEC_LOGIC_HIGH_US) < err_val->logic0_high_err_val) && \
        (ABS(data[i+1], NEC_LOGIC0_LOW_US) < err_val->logic0_low_err_val)) {
            // PR_DEBUG_RAW("0 ");
            if (is_msb) {
                nec_data[data_index] &= (~(1<<(7-data_cnt)));
            } else {
                nec_data[data_index] &= (~(1<<data_cnt));
            }
        } else if ((ABS(data[i], NEC_LOGIC_HIGH_US) < err_val->logic1_high_err_val) && \
        (ABS(data[i+1], NEC_LOGIC1_LOW_US) < err_val->logic1_low_err_val)) {
            // PR_DEBUG_RAW("1 ");
            if (is_msb) {
                nec_data[data_index] |= (1<<(7-data_cnt));
            } else {
                nec_data[data_index] |= (1<<data_cnt);
            }
        } else {
            TAL_PR_ERR("nec data decode error, %d, %d, %d, %d", data_cnt, i, data[i], data[i+1]);
            return i;
        }

        data_cnt++;
        if (data_cnt > 7) {
            data_cnt = 0;
            data_index++;
            // PR_DEBUG_RAW("\r\n");
        }
    }

    nec_code->addr = nec_data[0];
    nec_code->addr = nec_code->addr<<8 | nec_data[1];
    nec_code->cmd = nec_data[2];
    nec_code->cmd = nec_code->cmd<<8 | nec_data[3];

    /* repeat decode */
    decode_index += NEC_CODE_DATA_LEN;
    for (i=decode_index; i+3<len; i=i+4) {
        if ((ABS(data[i+2], NEC_REPEAT_HIGH_US) < err_val->repeat_high_err_val) && \
        (ABS(data[i+3], NEC_REPEAT_LOW_US) < err_val->repeat_low_err_val)) {
            nec_code->repeat_cnt++;
        } else {
            break;
        }
    }

    return i;
}

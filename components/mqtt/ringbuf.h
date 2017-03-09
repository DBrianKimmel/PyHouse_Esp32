#ifndef _RING_BUF_H_
#define _RING_BUF_H_

#include <stdint.h>

#include "esp_err.h"

typedef struct   Ringbuff {
	uint8_t*            p_o; /**< Original pointer */
	uint8_t* volatile   read_ptr; /**< Read pointer */
	uint8_t* volatile   write_ptr; /**< Write pointer */
	volatile int32_t    fill_cnt; /**< Number of filled slots */
	int32_t             size; /**< Buffer size */
	int32_t             block_size;
} Ringbuff_t;

esp_err_t rb_init(Ringbuff_t *r, uint8_t* buf, int32_t size, int32_t block_size);
int32_t rb_put(Ringbuff_t *r, uint8_t* c);
int32_t rb_get(Ringbuff_t *r, uint8_t* c);
int32_t rb_available(Ringbuff_t *r);
uint32_t rb_read(Ringbuff_t *r, uint8_t *buf, int len);
uint32_t rb_write(Ringbuff_t *r, uint8_t *buf, int len);

#endif

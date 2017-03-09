/**
 * @file ringbuf,c
 *
 *   Ring Buffer library
 */
#include <stdio.h>
#include <string.h>
#include "ringbuf.h"

#include "esp_log.h"

static const char *TAG = "RingBuff      ";



/**
 * @brief init a Ringbuff_t object
 * @param r pointer to a Ringbuff_t object
 * @param buf pointer to a byte array
 * @param size size of buf
 * @param block_size is size of data as block
 * @return 0 if successful, otherwise failed
 */
esp_err_t rb_init(Ringbuff_t *p_rb, uint8_t *p_buf, int32_t p_size, int32_t p_block_size) {
	if (p_rb == 0 || p_buf == 0 || p_size < 2) {
		ESP_LOGE(TAG, " 24 rb_init - Invalid Parameters RingBuffPtr:%p;  BufferPtr:%p;  Size:%d;  BlkSize:%d", p_rb, p_buf, p_size, p_block_size);
		return ESP_FAIL;
	}
	if (p_size % p_block_size != 0) {
		return ESP_FAIL;
	}
	p_rb->p_o = p_rb->read_ptr = p_rb->write_ptr = p_buf;
	p_rb->fill_cnt = 0;
	p_rb->size = p_size;
	p_rb->block_size = p_block_size;
	ESP_LOGI(TAG, "34 rb_init - Finished.");
	return ESP_OK;
}



/**
 * \brief put a character into ring buffer
 * \param r pointer to a ringbuf object
 * \param c character to be put
 * \return 0 if successfull, otherwise failed
 */
int32_t rb_put(Ringbuff_t *r, uint8_t *c) {
	int32_t i;
	uint8_t *data = c;
	if (r->fill_cnt >= r->size) {
		return -1; // ring buffer is full, this should be atomic operation
	}
	r->fill_cnt += r->block_size; // increase filled slots count, this should be atomic operation
	for (i = 0; i < r->block_size; i++) {
		*r->write_ptr = *data;              // put character into buffer

		r->write_ptr++;
		data++;
	}
	if (r->write_ptr >= r->p_o + r->size) {
		// rollback if write pointer go pass
		r->write_ptr = r->p_o;          // the physical boundary
	}
	return 0;
}



/**
 * @brief  get a character from ring buffer
 *
 * \param r pointer to a ringbuf object
 * \param c read character
 * \return 0 if successfull, otherwise failed
 */
int32_t rb_get(Ringbuff_t *r, uint8_t *c) {
	int32_t i;
	uint8_t *data = c;
	if (r->fill_cnt <= 0) {
		return -1;     // ring buffer is empty, this should be atomic operation
	}
	r->fill_cnt -= r->block_size;              // decrease filled slots count

	for (i = 0; i < r->block_size; i++) {
		*data++ = *r->read_ptr++;               // get the character out
	}
	if (r->read_ptr >= r->p_o + r->size) {      // rollback if write pointer go pass
		r->read_ptr = r->p_o;            // the physical boundary
	}
	return 0;
}



int32_t rb_available(Ringbuff_t *r) {
	return (r->size - r->fill_cnt);
}


/*
 *
 */
uint32_t rb_read(Ringbuff_t *r, uint8_t *buf, int len) {
	int  n = 0;
	uint8_t  data;
	while (len > 0) {
		while (rb_get(r, &data) != 0)
			;
		*buf++ = data;
		n++;
		len--;
	}
	return n;
}



/*
 * Put each byte of the buffer
 */
uint32_t rb_write(Ringbuff_t *r, uint8_t *buf, int len) {
	uint32_t wi;
	for (wi = 0; wi < len; wi++) {
		while (rb_put(r, &buf[wi]) != 0) {
		}
	}
	return 0;
}

// ### END DBK

#ifndef __K_BUFFER_H__
#define __K_BUFFER_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

class kBuffer {
public:
	kBuffer(int length = 8196);
	~kBuffer();

	void realloc(int length);
	int copy(uint8_t *data, int size);
	int read(uint8_t *data, int size);
	int write(uint8_t *data, int size);

	int availd(void);
	void reset();
	const uint8_t *data(void);
	

private:
	uint8_t *m_buffer;
	int m_length;
	int m_read;
	int m_write;
	int m_used;

	pthread_mutex_t m_mutex;
};

#endif

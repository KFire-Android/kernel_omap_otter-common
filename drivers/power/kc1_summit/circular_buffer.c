#include <linux/slab.h>
typedef struct
{
	int  *datas;
	int	 buffer_size;
	int	 start_index;
	int	 fill_count;
}circular_buffer_t;

void initCBuffer(circular_buffer_t *cbuffer , int size)
{
	int i;
	cbuffer->datas = NULL;
	cbuffer->datas = (int *)kmalloc((size*sizeof(int)), GFP_ATOMIC);
	if (NULL == cbuffer->datas)
		printk(KERN_ERR "%s:circular buffer failed allocation\n", __func__);

	for (i = 0; i < size; i++)
		cbuffer->datas[i] = -1;
	cbuffer->buffer_size = size;
	cbuffer->fill_count = 0;
	cbuffer->start_index = 0;
}
void releaseCBuffer(circular_buffer_t *cbuffer)
{   
	kfree(cbuffer->datas);
}
uint32_t readFromCBuffer(circular_buffer_t *cbuffer)
{
	uint32_t data;
	if (cbuffer->fill_count == 0)
		return 0xffff;
	data = cbuffer->datas[cbuffer->start_index];
	cbuffer->datas[cbuffer->start_index] = -1;
	if (cbuffer->start_index == (cbuffer->buffer_size-1))
		cbuffer->start_index = 0;
	else
		cbuffer->start_index++;
	cbuffer->fill_count--;
	return data;
}

void writeIntoCBuffer (circular_buffer_t *cbuffer , uint32_t data)
{
	int temp = 0;
	if ((cbuffer->start_index+cbuffer->fill_count) > (cbuffer->buffer_size-1)) {
		temp = cbuffer->start_index + cbuffer->fill_count-cbuffer->buffer_size;
		cbuffer->datas[temp] = data;
	} else
		cbuffer->datas[cbuffer->start_index+cbuffer->fill_count] = data;
	cbuffer->datas[cbuffer->start_index+cbuffer->fill_count] = data;
	if (cbuffer->fill_count == cbuffer->buffer_size)
		cbuffer->start_index++;
	else
		cbuffer->fill_count++;
}

int isCBufferNotEmpty(circular_buffer_t *cbuffer)
{
	if (cbuffer->fill_count == 0)
		return 0;
	else
		return 1;
}

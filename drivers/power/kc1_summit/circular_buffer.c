//#include <linux/vmalloc.h>
#include <linux/slab.h>
typedef struct
{
    int*    datas;
    int     buffer_size;
    int     start_index;
    int     fill_count;
    //spinlock_t      lock;
    
}circular_buffer_t;

void initCBuffer(circular_buffer_t* cbuffer,int size)
{
    //cbuffer->datas=(int*)vmalloc(size*sizeof(int));
    int i;
    cbuffer->datas = NULL;
    cbuffer->datas = (int *)kmalloc ((size*sizeof(int)), GFP_ATOMIC);
    if (NULL == cbuffer->datas)
    {
    	printk (KERN_ERR "%s:circular buffer failed allocation\n", __FUNCTION__);
    }

    for(i=0;i<size;i++)
    {
        cbuffer->datas[i]=-1;
    }
    cbuffer->buffer_size=size;
    cbuffer->fill_count=0;
    cbuffer->start_index=0;
    //spin_lock_init(&cbuffer->lock);
}
void releaseCBuffer(circular_buffer_t* cbuffer)
{   
    //vfree(cbuffer->datas);
    kfree (cbuffer->datas);
}
uint32_t readFromCBuffer(circular_buffer_t* cbuffer)
{
    unsigned long   flags;
    uint32_t data;
    //buffer empty
    //spin_lock_irqsave(&cbuffer->lock, flags);
    if(cbuffer->fill_count==0)
    {
        //spin_unlock_irqrestore(&cbuffer->lock, flags);
	// Cannot return 0, as it maps USB_EVENT_NONE,
	// Which seems to be taken equal to USB unplug
	// Hence setting it to 0xffff, which should not
	// map to any proper event
        return 0xffff;
    }

    data=cbuffer->datas[cbuffer->start_index];
    cbuffer->datas[cbuffer->start_index]=-1;
    //printk(" read <- index=%d data=%d ",cbuffer->start_index,data);
    if(cbuffer->start_index==(cbuffer->buffer_size-1))
    {
        cbuffer->start_index=0;
    }
    else
    {
        cbuffer->start_index++;
    }
    cbuffer->fill_count--;
    //printk("fill_count=%d\n",cbuffer->fill_count);
    //spin_unlock_irqrestore(&cbuffer->lock, flags);
    return data;
}

void writeIntoCBuffer (circular_buffer_t* cbuffer,uint32_t data)
{
    unsigned long   flags;
    int temp=0;
    //spin_lock_irqsave(&cbuffer->lock, flags);
    //int previous=(cbuffer->start_index+cbuffer->fill_count)-1;
    //if (previous < 0)
    //    previous=0;
   // if(cbuffer->datas[previous]!=data){
    if((cbuffer->start_index+cbuffer->fill_count)>(cbuffer->buffer_size-1))
    {
        temp=cbuffer->start_index+cbuffer->fill_count-cbuffer->buffer_size;
        cbuffer->datas[temp]=data;
        //printk(" write -> index=%d data=%d",temp,data);
    }else{
        cbuffer->datas[cbuffer->start_index+cbuffer->fill_count]=data;
        //printk(" write -> index=%d data=%d",cbuffer->start_index+cbuffer->fill_count,data);
    }
   
        cbuffer->datas[cbuffer->start_index+cbuffer->fill_count]=data;
        //if buffer is full
        if(cbuffer->fill_count==cbuffer->buffer_size)
        {
            cbuffer->start_index++;
        }
        else
        {
            cbuffer->fill_count++;
        }
    //printk("fill_count=%d\n",cbuffer->fill_count);
    //}
    //spin_unlock_irqrestore(&cbuffer->lock, flags);
}

int isCBufferNotEmpty(circular_buffer_t* cbuffer)
{
    if(cbuffer->fill_count==0)
        return 0;
    else
        return 1;
}

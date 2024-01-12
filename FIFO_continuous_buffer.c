#include "FIFO_continuous_buffer.h"
#include "string.h" //使用memcpy()函数

FIFO_continuous_buffer FIFO_buf_by_normal_buf(unsigned char *buf_ADR, unsigned int _size, unsigned int max_output_size)
{
    FIFO_continuous_buffer BUF;
    BUF.buffer = buf_ADR;
    BUF.I_ptr = buf_ADR;
    BUF.O_ptr = buf_ADR;
    BUF.begin_limit = buf_ADR + max_output_size - 1;
    BUF.last_limit = buf_ADR + _size - max_output_size - 1;
    BUF.map_input_size = _size - max_output_size;
    BUF.ERROR = 0;
    return BUF;
}
void FIFO_buffer_input_byte(FIFO_continuous_buffer *buf, const unsigned char dat)
{
    unsigned char *In = buf->I_ptr;

    *(In) = dat;
    if (In < buf->begin_limit)
        In[buf->map_input_size] = dat;

    if (In == buf->last_limit)
        In = buf->buffer;
    else
        In++;
    if (In == buf->O_ptr)
        buf->ERROR = 1;
    else
        buf->I_ptr = In;
}
unsigned int FIFO_buffer_input_many(FIFO_continuous_buffer *buf, unsigned char *buf_in, unsigned int len) //
{
    unsigned int O1 = buf->O_ptr - buf->buffer;
    unsigned int I1 = buf->I_ptr - buf->buffer;
    unsigned int I2 = buf->last_limit - buf->I_ptr + 1;
    unsigned int count = 0;
    if (I1 >= O1) // 写指针在尾，读指针还在头
    {
        if (len > I2) // 写数量超过结尾长度，需要回到开头继续写
        {
            memcpy(buf->I_ptr, buf_in, I2); // 写到结尾
            buf_in += I2;
            if (len - I2 < O1) // 开头剩余空间充足
            {
                count = len - I2;
                memcpy(buf->buffer, buf_in, count); // 写到设定长度
                buf->I_ptr = buf->buffer + count;
                if (buf->I_ptr < buf->begin_limit)                            // 在影子内存限位之前
                    memcpy(buf->buffer + buf->map_input_size, buf_in, count); // 复制一份到影子内存
                else
                    memcpy(buf->buffer + buf->map_input_size, buf_in, buf->begin_limit - buf->buffer); // 复制一份到影子内存
                return len;
            }
            else // 开头剩余空间不足,写到数据区满为止
            {
                count = O1 - 1;
                memcpy(buf->buffer, buf_in, count); // 写满的长度
                buf->I_ptr = buf->buffer + count;
                if (buf->I_ptr < buf->begin_limit)                            // 在影子内存限位之前
                    memcpy(buf->buffer + buf->map_input_size, buf_in, count); // 复制一份到影子内存
                else
                    memcpy(buf->buffer + buf->map_input_size, buf_in, buf->begin_limit - buf->buffer); // 复制一份到影子内存
                buf->ERROR = 1;
                return count + I2; // I2+O1-1;
            }
        }
        else
        {
            memcpy(buf->I_ptr, buf_in, len);
            buf->I_ptr = buf->I_ptr + len;
            return len; // I2+O1-1;
        }
    }
    else
    {
        unsigned int count_copy;
        count = O1 - I1;
        if (count > len)
        {
            count = len;
        }
        memcpy(buf->I_ptr, buf_in, count);
        if (buf->I_ptr < buf->begin_limit) //
        {
            if (buf->I_ptr + count < buf->begin_limit)
                count_copy = count;
            else
                count_copy = buf->begin_limit - buf->I_ptr + 1;
            memcpy(buf->buffer + buf->map_input_size, buf_in, count_copy); // 复制一份到影子内存
        }
        buf->I_ptr += count;
        buf->ERROR = 1;
        return count;
    }
}
/*从FIFO流中获取一片连续写入内存
    buf : FIFO流控结构体的地址
    buf_out: 待分配内存指针
    max_datas : 期望分配长度
    return:实际分配长度
    */
unsigned int FIFO_buffer_alloc_input(FIFO_continuous_buffer *buf, unsigned char **buf_in, unsigned int max_datas)
{
    int count = 0;
    *buf_in = buf->I_ptr;

    count = buf->O_ptr - buf->I_ptr - 1;
    if (count < 0)
    {
        count += buf->map_input_size;
    }
    if (count > max_datas)
        count = max_datas;

    if (count == 0)
        buf->ERROR = 1;
    return count;
}
/*向FIFO流回应内存写入完毕
    必须和FIFO_buffer_alloc_input配对使用
    buf : FIFO流控结构体的地址
    len : 之前读取的数据长度
    */
void FIFO_buffer_alloc_free_input(FIFO_continuous_buffer *buf, unsigned int len)
{
    unsigned char *I_buf = buf->I_ptr+len;
    if(buf->I_ptr<buf->begin_limit)
    {
        if(I_buf>buf->begin_limit)
            memcpy(buf->last_limit + 1,buf->I_ptr, buf->begin_limit - buf->I_ptr); //从开头复制一份数据到影子内存中
        else
            memcpy(buf->last_limit + 1,buf->I_ptr, len); //从开头复制一份数据到影子内存中
    }
    
    if (I_buf > buf->last_limit)
    {
        I_buf -= buf->map_input_size;
        memcpy(buf->buffer, buf->last_limit + 1, I_buf - buf->buffer); // 从影子内存中复制一份数据到开头
    }
    buf->I_ptr = I_buf;
}

/*从FIFO流中获取一片连续读取内存
    buf : FIFO流控结构体的地址
    buf_out: 待分配内存指针
    max_datas : 期望分配长度
    return:实际分配长度
    */
unsigned int FIFO_buffer_alloc_output(FIFO_continuous_buffer *buf, unsigned char **buf_out, unsigned int max_datas)
{
    int count = 0;
    *buf_out = buf->O_ptr;
    if (buf->O_ptr == buf->I_ptr)
        buf->ERROR = 2;
    else
    {
        count = buf->I_ptr - buf->O_ptr;
        if (count < 0)
        {
            count += buf->map_input_size;
        }

        if (count > max_datas)
            count = max_datas;
    }
    return count;
}

/*向FIFO流回应内存读取完毕
buf : FIFO流控结构体的地址
len : 之前读取的数据长度
*/
void FIFO_buffer_alloc_free_output(FIFO_continuous_buffer *buf, unsigned int len)
{
    unsigned char *O_buf = buf->O_ptr;
    O_buf += len;
    if (O_buf > buf->last_limit)
    {
        O_buf -= buf->map_input_size;
    }
    buf->O_ptr = O_buf;
}

unsigned char FIFO_buffer_output_byte(FIFO_continuous_buffer *buf) //
{
    unsigned char *Out = buf->O_ptr;
    if (buf->O_ptr == buf->I_ptr)
        buf->ERROR = 2;
    else
    {
        if (Out == buf->last_limit)
            buf->O_ptr = buf->buffer;
        else
            buf->O_ptr = Out + 1;
    }
    return *Out;
}

unsigned int FIFO_buffer_length(FIFO_continuous_buffer *buf) //
{
    unsigned int O1 = buf->O_ptr - buf->buffer;
    unsigned int O2 = buf->last_limit - buf->O_ptr + 1;
    unsigned int I1 = buf->I_ptr - buf->buffer;
    if (I1 >= O1)
    {
        return I1 - O1;
    }
    else
    {
        return I1 + O2;
    }
}
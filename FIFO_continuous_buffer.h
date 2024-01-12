#ifndef __FIFO_CONTINUOUS_BUFFER_H
#define __FIFO_CONTINUOUS_BUFFER_H
/*

初始化：
        首先定义一个一维数组BUF，将其地址，长度和最长数据帧长度交给FIFO_buf_by_normal_buf函数，生成一个控制结构。最长数据帧
    长度不超过BUF长度的一半，BUF可以设大些，数据帧长度也可以留冗余。

简便用法：
        中断内使用单字节快速写入（FIFO_buffer_fast_input）。需要读取时先判断FIFO长度足够长，然后直接分配出读取内存指针。
        分配出去一段空间并不意味着会失去它，使用者完全可以读了100B的空间，但要求FIFO释放50B，这样做的好处是可以在找到帧尾
    后，使FIFO刚好弹出这一帧，不影响下次读取。有利于不定长数据的分解。
        也就是说分配函数是可以随时使用的（注意其返回值是实际分配长度）。对于释放函数，输出释放函数直接使用会清除一部分FIFO，
    输入释放函数直接使用会导致内存区数据错误。

底层原理：
        此库中代码实现了一种FIFO，基于已存在的一维数组做内存区，以环形FIFO的流控方法处理数据。此FIFO拥有向外分配一段
    输入或输出连续内存区的指针的功能，可以被传递给DMA或者其他程序，当输入或输出完毕使需要使用释放函数表示输入或输出完成。
        为了实现这样的功能，FIFO中有一段影子内存，此内存是用于输入和输出内存的中转区，其大小由用户指定，请求分配的连续
    内存不得超过影子内存大小，否则会出现错误。并且设定影子内存的大小不得超过总内存大小的一半，否则会出现未知错误。
        影子内存位于内存区结尾，其内容将和开头处相同大小的内存区内容保持一致，保持一致的复制过程在写入时完成，对于单字节
    写入，增加的时间并不长，对于多字节写入函数的判断分支很多，实际执行长度也不长，不占用太多时间，但维护起来很复杂。            （言外之意可能存在BUG）
        因为影子内存区域的内容需要多写一份，所以当总内存区大小:影子内存大小的比值越大，需要产生复制动作的占比就越少，花费
    时间就越接近单个写入。影子内存刚好是总内存一半时，每次写入都要多写一份数据，是极端情况。（实际上单字节写入耗费时间并不
    只花费在写入上，多写一份的影响不是两倍）
    */
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct _FIFO_continuous_buffer
    {
        unsigned char *buffer;      // 数据区地址
        unsigned char *O_ptr;       // 读取指针地址
        unsigned char *I_ptr;       // 写入指针地址
        unsigned char *begin_limit; // 开头指针限位地址
        unsigned char *last_limit;  // 结尾指针限位地址
        unsigned int map_input_size;
        unsigned char ERROR; // 记录错误的变量：0:无错误，1:写入时发现数据区全满，2:读取时发现数据队列为空
    } FIFO_continuous_buffer;

    /*统计FIFO中已存放的数据长度
    return: 数据长度
    */
    unsigned int FIFO_buffer_length(FIFO_continuous_buffer *buf);

    /*宏定义:查看FIFO中是否有数据,必须以逻辑值使用(在if,while,for中使用)
    FIFO:   FIFO流控结构体
    return: 逻辑假:无数据，逻辑真:有数据
    */
#define if_FIFO_buffer_have_data(FIFO) (FIFO.I_ptr != FIFO.O_ptr)

    /*对输入的一维数组创造一个FIFO流控方法，实际数据保存在数组中，流控指针保存在FIFO结构中。
    buf_ADR :   存储实际数据的一维数组
    _size :     数组长度
    max_output_size:影子内存长度，不得超过_size的一半(建议为4的整倍数)
    */
    FIFO_continuous_buffer FIFO_buf_by_normal_buf(unsigned char *buf_ADR, unsigned int _size, unsigned int max_output_size);

    /*向FIFO流中放入一个字节
    buf : FIFO流控结构体的地址
    dat : 数据
    */
    void FIFO_buffer_input_byte(FIFO_continuous_buffer *buf, const unsigned char dat);

    /*宏函数:快速在FIFO流中放入一个字节，可用于中断的快速处理
    FIFO:   FIFO流控结构体
    dat:    放入的数据
    */
#define FIFO_buffer_fast_input(FIFO, dat)          \
    {                                              \
        unsigned char *In = FIFO.I_ptr;            \
        if (In < FIFO.begin_limit)                 \
            In[FIFO.map_input_size] = *(In) = dat; \
        else                                       \
            *(In) = dat;                           \
        if (In == FIFO.last_limit)                 \
            In = FIFO.buffer;                      \
        else                                       \
            In++;                                  \
        if (In == FIFO.O_ptr)                      \
            FIFO.ERROR = 1;                        \
        else                                       \
            FIFO.I_ptr = In;                       \
    }

    /*从FIFO流中获取一片连续读取内存，不得超过影子内存长度(多次获取以实现大量读取）当返回值不是期望大小时说明内存区已满
    buf : FIFO流控结构体的地址
    buf_out: 待分配内存指针的地址
    max_datas : 期望分配长度
    return:实际分配长度
    */
    unsigned int FIFO_buffer_alloc_output(FIFO_continuous_buffer *buf, unsigned char **buf_out, unsigned int max_datas);

    /*向FIFO流回应内存读取完毕
    buf : FIFO流控结构体的地址
    len : 之前读取的数据长度
    */
    void FIFO_buffer_alloc_free_output(FIFO_continuous_buffer *buf, unsigned int len);

    /*从FIFO流中取出一个字节
    buf :   FIFO流控结构体的地址
    return: 取出的数据
    */
    unsigned char FIFO_buffer_output_byte(FIFO_continuous_buffer *buf);

    /*宏函数:快速在FIFO流中取出一个字节，可用于中断的快速处理
    FIFO:   FIFO流控结构体
    return_dat: 取出的数据
    */
#define FIFO_buffer_fast_output(FIFO, return_dat) \
    {                                             \
        unsigned char *Out = FIFO.O_ptr;          \
        if (FIFO.O_ptr == FIFO.I_ptr)             \
            FIFO.ERROR = 2;                       \
        else                                      \
        {                                         \
            if (Out == FIFO.last_limit)           \
                FIFO.O_ptr = FIFO.buffer;         \
            else                                  \
                FIFO.O_ptr = Out + 1;             \
        }                                         \
        return_dat = *Out;                        \
    }

    /*向FIFO流中复制多个字节，不得超过FIFO总大小-影子内存长度(一次性大量复制,建议为4的整倍数)
    buf :       FIFO流控结构体的地址
    buf_in :    输入的多字节数组
    len :       输入长度
    return :    实际写入的长度
    */
    unsigned int FIFO_buffer_input_many(FIFO_continuous_buffer *buf, unsigned char *buf_in, unsigned int len);

    /*从FIFO流中获取一片连续写入内存，不得超过影子内存长度(多次分配以实现大量写入）当返回值不是期望大小时说明内存区已满
    必须和FIFO_buffer_alloc_free_input配对使用
    buf : FIFO流控结构体的地址
    buf_out: 待分配内存指针的地址
    max_datas : 期望分配长度
    return:实际分配长度
    */
    unsigned int FIFO_buffer_alloc_input(FIFO_continuous_buffer *buf, unsigned char **buf_in, unsigned int max_datas);

    /*向FIFO流回应内存写入完毕
    必须和FIFO_buffer_alloc_input配对使用
    buf : FIFO流控结构体的地址
    len : 之前读取的数据长度
    */
    void FIFO_buffer_alloc_free_input(FIFO_continuous_buffer *buf, unsigned int len);
#ifdef __cplusplus
}
#endif

#endif // !__FIFO_CONTINUOUS_BUFFER_H

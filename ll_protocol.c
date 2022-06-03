#include "ll_protocol.h"


size_t ll_sizeof_serialized(ll_message_info_t msg_info, const uint8_t* data)
{
    if(!data)
    {
        return 0;
    }
    //+2 is for msg_info.begin_byte at the beginning and msg_info.end_byte at the end of message
    size_t result = msg_info.size + 2;
    for(size_t i = 0; i < msg_info.size; i++)
    {
        if(   data[i] == msg_info.begin_byte
           || data[i] == msg_info.end_byte
           || data[i] == msg_info.reject_byte)
        {
            result++;
        }
    }
    return result;
}

void ll_serialize(ll_message_info_t msg_info, const uint8_t* data_in, uint8_t* data_out)
{
    if(!data_in || !data_out)
    {
        return;
    }

    uint8_t* tmp_out = data_out;

    *data_out = msg_info.begin_byte;
    tmp_out++;

    for(size_t i = 0; i < msg_info.size; i++)
    {
        if(   data_in[i] == msg_info.begin_byte
           || data_in[i] == msg_info.end_byte
           || data_in[i] == msg_info.reject_byte)
        {
            *tmp_out = msg_info.reject_byte;
            tmp_out++;
            *tmp_out = data_in[i];
            tmp_out++;
        }
        else
        {
            *tmp_out = data_in[i];
            tmp_out++;
        }
    }
    *tmp_out = msg_info.end_byte;
}

ll_status_t ll_deserialize(ll_message_info_t msg_info,
                           const uint8_t* byte_stream, 
                           size_t byte_stream_size, 
                           uint8_t* data_out,
                           size_t* remainder)
{
    if(!byte_stream || !data_out || !remainder)
    {
        return LL_STATUS_BAD_PARAMS;
    }

    *remainder = 0;
    bool message_opened = false;
    bool reject = false;
    bool ignore_previous = false;
    size_t message_iter = 0;
    uint8_t byte = msg_info.end_byte;
    uint8_t previous_byte = msg_info.end_byte;

    for(size_t i = 0; i < byte_stream_size; i++)
    {
        previous_byte = byte;
        byte = byte_stream[i];

        if(  !message_opened
           && byte == msg_info.begin_byte
           && previous_byte != msg_info.reject_byte)
        {
            message_opened = true;
        }

        if(!message_opened)
        {
            continue;
        }

        if(message_iter == msg_info.size)
        {
            message_opened = false;
            if(byte == msg_info.end_byte)
            {
                if(i == byte_stream_size - 1)
                {
                    *remainder = 0;
                }
                else
                {
                    *remainder = i + 1;
                }
                return LL_STATUS_SUCCESS;
            }
            else
            {
                *remainder = i;
                return LL_STATUS_MESSAGE_TOO_LONG;
            }
            message_iter = 0;
            continue;
        }

        if(   byte == msg_info.end_byte
           && previous_byte != msg_info.reject_byte
           && !ignore_previous
           && message_iter < msg_info.size)
        {
            message_opened = false;
            reject = false;
            message_iter = 0;
            *remainder = i + 1;
            return LL_STATUS_MESSAGE_TOO_SHORT;
        }

        //it seems that it can be optimized
        //by combining this "if()"
        if(   (byte != msg_info.reject_byte
           && byte != msg_info.begin_byte
           && byte != msg_info.end_byte
           && previous_byte != msg_info.reject_byte)
           ||
             (byte != msg_info.reject_byte
           && byte != msg_info.begin_byte
           && byte != msg_info.end_byte
           && ignore_previous))
        {
            data_out[message_iter++] = byte;
            ignore_previous = false;
            continue;
         }

        //with this "if()"
        if(reject)
        {
            data_out[message_iter++] = byte;
            if(byte == msg_info.reject_byte)
            {
                ignore_previous = true;
            }
            reject = false;
            continue;
        }

        if(byte == msg_info.reject_byte && (previous_byte != msg_info.reject_byte || ignore_previous))
        {
            reject = true;
            if(ignore_previous)
            {
                ignore_previous = false;
            }
        }
    }

    if(message_opened)
    {
        *remainder = byte_stream_size - message_iter - 1;
        return LL_STATUS_NO_ENOUGH_BYTES;
    }

    *remainder = byte_stream_size;
    return LL_STATUS_NO_MESSAGE;
}

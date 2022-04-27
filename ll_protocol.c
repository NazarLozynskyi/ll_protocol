#include "ll_protocol.h"


static uint8_t message[MESSAGE_SIZE];


size_t ll_sizeof_serialized(uint8_t* const data)
{
    //+2 is for BEGIN_BYTE at the beginning and END_BYTE at the end of message
    size_t result = MESSAGE_SIZE + 2;
    for(size_t i = 0; i < MESSAGE_SIZE; i++)
    {
        if(   data[i] == BEGIN_BYTE
           || data[i] == END_BYTE
           || data[i] == REJECT_BYTE)
        {
            result++;
        }
    }
    return result;
}

bool ll_serialize(uint8_t* const data_in, uint8_t* const data_out)
{
    if(!data_in || !data_out)
    {
        return false;
    }

    uint8_t* tmp_out = data_out;

    *data_out = BEGIN_BYTE;
    tmp_out++;

    for(size_t i = 0; i < MESSAGE_SIZE; i++)
    {
        if(   data_in[i] == BEGIN_BYTE
           || data_in[i] == END_BYTE
           || data_in[i] == REJECT_BYTE)
        {
            *tmp_out = REJECT_BYTE;
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
    *tmp_out = END_BYTE;
    return true;
}

bool ll_deserialize(uint8_t* const byte_stream, size_t byte_stream_size, size_t* const start_of_remainder)
{
    if(byte_stream == NULL || start_of_remainder == NULL)
    {
        return false;
    }

    *start_of_remainder = 0;
    bool message_opened = false;
    bool reject = false;
    bool ignore_previous = false;
    size_t message_iter = 0;
    uint8_t byte = END_BYTE;
    uint8_t previous_byte = END_BYTE;

    for(size_t i = 0; i < byte_stream_size; i++)
    {
        previous_byte = byte;
        byte = byte_stream[i];

        if(  !message_opened
           && byte == BEGIN_BYTE
           && previous_byte != REJECT_BYTE)
        {
            message_opened = true;
        }

        if(!message_opened)
        {
            continue;
        }

        if(message_iter == MESSAGE_SIZE)
        {
            message_opened = false;
            if(byte == END_BYTE)
            {
                ll_message_ready_cb(message);
                *start_of_remainder = i + 1;
            }
            else
            {
                ll_error_cb(LL_PROTOCOL_ERR_MESSAGE_TOO_LONG);
                *start_of_remainder = i;
            }
            message_iter = 0;
            continue;
        }

        if(   byte == END_BYTE
           && previous_byte != REJECT_BYTE
           && !ignore_previous
           && message_iter < MESSAGE_SIZE)
        {
            message_opened = false;
            reject = false;
            message_iter = 0;
            *start_of_remainder = i + 1;
            ll_error_cb(LL_PROTOCOL_ERR_MESSAGE_TOO_SHORT);
        }

        //it seems that it can be optimized
        //by combining this "if()"
        if(   (byte != REJECT_BYTE
           && byte != BEGIN_BYTE
           && byte != END_BYTE
           && previous_byte != REJECT_BYTE)
           ||
             (byte != REJECT_BYTE
           && byte != BEGIN_BYTE
           && byte != END_BYTE
           && ignore_previous))
        {
            message[message_iter++] = byte;
            ignore_previous = false;
            continue;
         }

        //with this "if()"
        if(reject)
        {
            message[message_iter++] = byte;
            if(byte == REJECT_BYTE)
            {
                ignore_previous = true;
            }
            reject = false;
            continue;
        }

        if(byte == REJECT_BYTE && (previous_byte != REJECT_BYTE || ignore_previous))
        {
            reject = true;
            if(ignore_previous)
            {
                ignore_previous = false;
            }
        }
    }

    return true;
}

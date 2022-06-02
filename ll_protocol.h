/*
    This code is written in C language to make it portable.
Purpose of this code is to catch separate messages from the
bytes stream. That is done by serializing message on the
transmitter node and deserializing bytes stream on the receiver node.
Serializing means adding some extra bytes to message which
help to detect begin and the end of message in the bytes stream. These
bytes are BEGIN_BYTE, END_BYTE and REJECT_BYTE. They are called
"control bytes". BEGIN_BYTE is added at the beginning of message,
END_BYTE at the end of message, REJECT_BYTE is used to differentiate
control bytes from message bytes.

    You can define control bytes as you want but they must be
different from each other. For example, BEGIN_BYTE can be equal to
0x56, END_BYTE 0x65 and REJECT_BYTE 0xFF and so on. Defining
your own values for control bytes can be useful when the message
that you must serialize contains a lot of bytes which values are
the same as values of control bytes. That means that you can
decrease the redundancy. You can understand it better in the
examples below.

    NOTE. There is also control of message size. That means that
you can't serialize and deserialize a message with size (quantity of bytes)
that is not equal to MESSAGE_SIZE. MESSAGE_SIZE can be defined by you.
MESSAGE_SIZE must be equal both on transmitter and receiver nodes.

Examples (for MESSAGE_SIZE 16 bytes, BEGIN_BYTE 0xAA, END_BYTE 0xBB
and REJECT_BYTE 0xCC):

1. Message that we need to serialize and transmit:
   F3 77 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31

   Bytes stream after serializing:
   AA F3 77 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31 BB

   As you can see, resulting bytes stream that will be transmitted
   contains 18 bytes. More than MESSAGE_SIZE. So, the rendundancy for
   the message in this example is 2 bytes. As you can see BEGIN_BYTE
   added at the beginning of message and END_BYTE at the end of message.

2. Message that we need to serialize and transmit (mention that
   second byte has the same value as END_BYTE):
   F3 BB 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31

   Bytes stream after serializing:
   AA F3 CC BB 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31 BB

   Resulting bytes stream is 19 bytes. Rendundancy is 3 bytes. REJECT_BYTE
   is added before second byte of message to inform deserializator that
   this byte is not control byte. Without REJECT_BYTE deserializator
   would interpret that byte as the end of message.

3. Message that we need to serialize and transmit (mention that
   second byte has the same value as REJECT_BYTE):
   F3 CC 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31

   Bytes stream after serializing:
   AA F3 CC CC 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31 BB

   Resulting bytes stream is 19 bytes. Rendundancy is 3 bytes. REJECT_BYTE
   is added before second byte of message to inform deserializator that
   this byte is not control byte. Without REJECT_BYTE deserializator
   would interpret that byte as control byte.

4. Other examples.
   1)
   input:  F3 BB AA C4 95 CC 76 8B 12 CC 34 DD AA 77 51 BB
   output: AA F3 CC BB CC AA C4 95 CC CC 76 8B 12 CC CC 34 DD CC AA 77 51 CC BB BB
   bytes stream 24 bytes, redundancy 8 bytes

   2)
   input:  CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC
   output: AA CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC BB
   bytes stream 34 bytes, redundancy 18 bytes

   3)
   input:  AA AA AA AA AA AA AA AA AA AA AA AA AA AA AA AA
   output: AA CC AA CC AA CC AA CC AA CC AA CC AA CC AA CC AA CC AA CC AA CC AA CC AA CC AA CC AA CC AA CC AA BB
   bytes stream 34 bytes, redundancy 18 bytes

   4)
   input:  BB BB BB BB BB BB BB BB BB BB BB BB BB BB BB BB
   output: AA CC BB CC BB CC BB CC BB CC BB CC BB CC BB CC BB CC BB CC BB CC BB CC BB CC BB CC BB CC BB CC BB BB
   bytes stream 34 bytes, redundancy 18 bytes

   As you can see, the more bytes in message, which values are equal to values
of control bytes, the more redundancy it causes. The redundancy can't be more
than MESSAGE_SIZE * 2 + 2 (examples 4.2, 4.3, 4.4).

WARNING. There is one nuance. Imagine that we trying to send two messages in serial:
MESSAGE ONE: DD DD DD DD DD DD CC DD DD DD DD DD
MESSAGE TWO: DD DD DD DD DD DD DD DD DD DD DD DD
Result bytes stream: AA DD DD DD DD DD DD CC CC DD DD DD DD DD BB AA DD DD DD DD DD DD DD DD DD DD DD DD BB

Than imagine situation:
Transmitter is sending AA DD DD DD DD DD DD CC -> some distortion happened -> transmitter start sending next message.

So we get such (from the point of view of the receiver) bytes stream:
AA DD DD DD DD DD DD CC AA DD DD DD DD DD DD DD DD DD DD DD DD BB

Error for too long message will be detected and we will lose second message because
start byte of second message will be interpreted as byte that must be rejected.


Example for code use:

//define callback
void ll_message_ready_cb(uint8_t* const message)
{
    printf("ready: ");
    for(size_t i = 0; i < MESSAGE_SIZE; i++)
    {
        printf("%02X ", message[i]);
    }
    printf("\n");
}

//define callback
void ll_error_cb(ll_protocol_err_t error)
{
    if(error == LL_PROTOCOL_ERR_MESSAGE_TOO_SHORT)
    {
        printf("LL_PROTOCOL_ERR_MESSAGE_TOO_SHORT\n");
    }
    if(error == LL_PROTOCOL_ERR_MESSAGE_TOO_LONG)
    {
        printf("LL_PROTOCOL_ERR_MESSAGE_TOO_LONG\n");
    }
}

void transmitter_node()
{
    uint8_t data[MESSAGE_SIZE] =
    {
        0xCC, 0xCC, 0xDD, 0xDD,
        0xDD, 0xDD, 0xDD, 0xDD,
        0xDD, 0xDD, 0xDD, 0xDD,
        0xDD, 0xDD, 0xDD, 0xDD
    };

    size_t byte_stream_size = ll_sizeof_serialized(data);
    uint8_t* byte_stream = (uint8_t*)malloc(byte_stream_size);
    ll_serialize(data, byte_stream);

    transmit(byte_stream, byte_stream_size);
}

void receiver_node(uint8_t* byte_stream, size_t byte_stream_size)
{
    size_t _remainder;
    ll_deserialize(byte_stream, byte_stream_size, &_remainder);
    //or you can implement calling of ll_deserialize in loop
}

*/

#ifndef LL_PROTOCOL_H
#define LL_PROTOCOL_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define MESSAGE_SIZE          (size_t)2
#define MAX_BYTE_STREAM_SIZE  (size_t)(MESSAGE_SIZE * 2 + 2)

#define BEGIN_BYTE    (uint8_t)0xAA
#define END_BYTE      (uint8_t)0xBB
#define REJECT_BYTE   (uint8_t)0xCC

typedef enum
{
    LL_PROTOCOL_ERR_MESSAGE_TOO_SHORT, //message has started with BEGIN_BYTE but has ended too early with END_BYTE
    LL_PROTOCOL_ERR_MESSAGE_TOO_LONG   //message started with BEGIN_BYTE but hasn't end with END_BYTE after last byte of message came
} ll_protocol_err_t;


/*
 * This function is used to know how many bytes you need
 * to reserve for serialized data.
 *
 * Returns quantity of bytes which must be reserved for "data_out"
 * in "ll_serialize".
 *
 * NOTE. "ll_sizeof_serialized" will parse area of memory
 * with size MESSAGE_SIZE to which parameter "data" points.
 */
size_t ll_sizeof_serialized(
    uint8_t* const data         // data which will be serialized in "ll_serialize"
);

/*
 * This function serializes "data_in" and puts the result to "data_out".
 *
 * Returns "true" on success and "false" on fail.
 *
 * NOTE. "data_in" size must be equal to MESSAGE_SIZE and "data_out"
 * size must by equal to size which was returned by ll_sizeof_serialized
 * for data_in.
 */
bool ll_serialize(
    uint8_t* const data_in,     // data which will be serialized
    uint8_t* const data_out     // array where result must be put
);

/**
 * @brief This function parses bytes stream. When message is detected it
 * calls "data_ready_cb" function. When error in bytes stream
 * detected it calls "error_cb" function.
 *
 * If in bytes stream remain bytes which were not parsed - this
 * function writes in "start_of_remainder" position in "byte_stream"
 * where these bytes started. If all bytes from bytes stream were parsed
 * then "start_of_remainder" must be equal to [byte_stream_size + 1]. If
 * none of the bytes in bytes stream were parsed then "start_of_remainder"
 * equals to zero. NOTE. There can be only two situations with remainder:
 * 1) none of the bytes were parsed; 2) first bytes was parsed and last remains.
 * That means that the end of remainder always ends in [byte_stream_size - 1]
 * position and can't be less than [byte_stream_size - 1].
 *
 * Logic of remainder manipulating must be implemented by you. This function
 * only points to position of start of remainder.
 *
 * @param byte_stream byte stream which must be deserialized
 * @param byte_stream_size byte stream size
 * @param start_of_remainder position of start of remainder in "byte_stream"
 * after deserializing
 * @param context context for multitasking
 *
 * @param data_ready_cb this function is called by "ll_deserialize" every time
 * when new message is detected, if data_ready_cb == NULL - do nothing
 *
 * @param error_cb this function is called by "ll_deserialize" every time when
 * error in bytes stream but errors such as wrong bytes between messages which
 * not started with BEGIN_BYTE will not be catched, if error_cb == NULL - do nothing
 *
 * @return "true" on success and "false" on fail.
 */
bool ll_deserialize(
    uint8_t* byte_stream,
    size_t byte_stream_size,
    size_t* start_of_remainder,
    void* context,
    void (*data_ready_cb)(
        uint8_t* message,          //incoming message to process with size of MESSAGE_SIZE
        void* context              //context for multitasking
    ),
    void (*error_cb)(
        ll_protocol_err_t error,   //type of error
        void* context              //context for multitasking
    )
);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // LL_PROTOCOL_H

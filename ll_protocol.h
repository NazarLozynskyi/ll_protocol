/*
    Purpose of this code is to catch separate messages from the
bytes stream. That is done by serializing message on the
transmitter node and deserializing bytes stream on the receiver node.
Serializing means adding some extra bytes to message which
help to detect begin and the end of message in the bytes stream. These
bytes are "begin byte", "end byte" and "reject byte". They are called
"control bytes". "begin byte" is added at the beginning of message,
"end byte" at the end of message, "reject byte" is used to differentiate
control bytes from message bytes.

    You can define control bytes as you want but they must be
different from each other. For example, "begin byte" can be equal to
0x56, "end byte" 0x65 and "reject byte" 0xFF and so on. Defining
your own values for control bytes can be useful when the message
that you must serialize contains a lot of bytes which values are
the same as values of control bytes. That means that you can
decrease the redundancy. You can understand it better in the
examples below.

    NOTE. There is also control of message size. That means that
you can't serialize and deserialize a message with size (quantity of bytes)
that is not equal to message size. Message size can be defined by you.
Message size must be equal both on transmitter and receiver nodes.

Examples (for message size 16 bytes, "begin byte" 0xAA, "end byte" 0xBB
and "reject byte" 0xCC):

1. Message that we need to serialize and transmit:
   F3 77 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31

   Bytes stream after serializing:
   AA F3 77 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31 BB

   As you can see, resulting bytes stream that will be transmitted
   contains 18 bytes. More than message size. So, the rendundancy for
   the message in this example is 2 bytes. Also you can see that "begin byte"
   added at the beginning of message and "end byte" at the end of message.

2. Message that we need to serialize and transmit (mention that
   second byte has the same value as "end byte"):
   F3 BB 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31

   Bytes stream after serializing:
   AA F3 CC BB 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31 BB

   Resulting bytes stream is 19 bytes. Rendundancy is 3 bytes. "reject byte"
   is added before second byte of message to inform deserializator that
   this byte is not control byte. Without "reject byte" deserializator
   would interpret that byte as the end of message.

3. Message that we need to serialize and transmit (mention that
   second byte has the same value as "reject byte"):
   F3 CC 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31

   Bytes stream after serializing:
   AA F3 CC CC 56 C4 95 94 76 8B 12 88 34 DD 44 77 51 31 BB

   Resulting bytes stream is 19 bytes. Rendundancy is 3 bytes. "reject byte"
   is added before second byte of message to inform deserializator that
   this byte is not control byte. Without "reject byte" deserializator
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
than message size * 2 + 2 (examples 4.2, 4.3, 4.4).

WARNING. There is one nuance. Imagine that we trying to send two messages in serial:
MESSAGE ONE: DD DD DD DD DD DD CC DD DD DD DD DD
MESSAGE TWO: DD DD DD DD DD DD DD DD DD DD DD DD
Result bytes stream: AA DD DD DD DD DD DD CC CC DD DD DD DD DD BB AA DD DD DD DD DD DD DD DD DD DD DD DD BB

And now imagine situation:
Transmitter is sending AA DD DD DD DD DD DD CC -> some distortion happened -> transmitter start sending next message.

So we get such (from the point of view of the receiver) bytes stream:
AA DD DD DD DD DD DD CC AA DD DD DD DD DD DD DD DD DD DD DD DD BB

Error for too long message will be detected and we will lose second message because
start byte of second message will be interpreted as byte that must be rejected.


Example for code use:
@todo

*/

#ifndef LL_PROTOCOL_H
#define LL_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>


typedef enum
{
    LL_STATUS_SUCCESS,           //success
    LL_STATUS_BAD_PARAMS,        //bad parameters
    LL_STATUS_NO_MESSAGE,        //there are no message in byte stream
    LL_STATUS_NO_ENOUGH_BYTES,   //byte stream end in the middle of message
    LL_STATUS_MESSAGE_TOO_SHORT, //message has started with "begin byte" but has ended too early with "end byte"
    LL_STATUS_MESSAGE_TOO_LONG,  /*message started with "begin byte" but hasn't end with "end byte" after last 
                                   byte of message came*/
    LL_STATUS_ENUM_SIZE          //enum size
} ll_status_t;

typedef struct
{
    size_t  size;        //message size
    uint8_t begin_byte;  //begin byte
    uint8_t reject_byte; //reject byte
    uint8_t end_byte;    //end byte
} ll_message_info_t;


/**
 * @brief This function is used to know how many bytes you need
 * to reserve for serialized data.
 * @param msg_info message info
 * @param data area of memory with size of msg_info.message_size which will be parsed
 * if data == NULL then function does nothing and returns 0
 * @returns quantity of bytes which must be reserved for "data_out" in "ll_serialize"
 */
size_t ll_sizeof_serialized(ll_message_info_t msg_info, const uint8_t* data);

/**
 * @brief This function serializes "data_in" and puts the result to "data_out".
 *
 * @param msg_info message info
 * @param data_in area of memory with size of msg_info.message_size which will be parsed, 
 * if data_in == NULL then function does nothing
 * @param data_out area of memory with size that was returned by ll_sizeof_serialized,
 * if data_out == NULL then function does nothing
 */
void ll_serialize(ll_message_info_t msg_info, const uint8_t* data_in, uint8_t* data_out);

/**
 * @brief This function parses bytes stream and puts the result to "data_out". When 
 * function ends parsing, it writes position of remainder to "remainder" poiner and 
 * returns status of parsing.
 * 
 * There can be only six cases in function behaviour:
 * 
 * 1. There are no one sequence of bytes started with "begin byte". 
 * Behaviour: function writes to "status" LL_STATUS_NO_MESSAGE and returns "byte_stream_size".
 * 
 * 2. There is a sequence of bytes started with "begin byte" but "end byte" come
 * too early. In other words - message is too short. 
 * Behaviour: function writes to "status" LL_STATUS_MESSAGE_TOO_SHORT and returns position of
 * next byte after "end byte" of broken message.
 * 
 * 3. There is a sequence of bytes started with "begin byte" but "end byte" doesn't come in time. 
 * In other words - message is too long.
 * Behaviour: function writes to "status" LL_STATUS_MESSAGE_TOO_LONG and returns position where 
 * "end byte" should have been.
 * 
 * 4. There is a sequence of bytes started with "begin byte" but in the middle of sequence 
 * we reach the end of byte stream.
 * Behaviour: function writes to reminder position of "start byte" of uncompleted message and
 * returns LL_STATUS_NO_ENOUGH_BYTES.
 * 
 * 5. Message was normally parsed and there are remaining bytes.
 * Behaviour: function writes to "remainder" position of next byte after "end byte" of parsed message 
 * and returns LL_STATUS_SUCCESS.
 * 
 * 6. Message was normally parsed and there are no remaining bytes.
 * Behaviour: function writes 0 to "remainder" and returns LL_STATUS_SUCCESS.
 * 
 * @note During parsing byte stream function writes result to "data_out". It means that
 * "data_out" always will be modified except of first case said above.
 * 
 * @note Function ignores all bytes sequences between messages in byte stream. It is about bytes 
 * sequences which are not started with "begin byte". That means that there will not be any
 * information about these lost bytes.
 * 
 * @todo Catch information about lost bytes.
 * @todo Rewrite logic to make it closer to repetitive calling of this function.
 *
 * @param msg_info message info
 * @param byte_stream area of memory with size of byte_stream_size which will be parsed, 
 * if byte_stream == NULL then function does nothing and returns LL_STATUS_BAD_PARAMS 
 * @param byte_stream_size byte stream size
 * @param data_out area of memory with size of msg_info.message_size where result will be putted, 
 * if data_out == NULL then function does nothing and returns LL_STATUS_BAD_PARAMS
 * @param remainder pointer to remainder,
 * if remainder == NULL then function does nothing and returns LL_STATUS_BAD_PARAMS
 * @returns status
 */
ll_status_t ll_deserialize(
    ll_message_info_t msg_info,
    const uint8_t* byte_stream, 
    size_t byte_stream_size, 
    uint8_t* data_out,
    size_t* remainder
);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // LL_PROTOCOL_H

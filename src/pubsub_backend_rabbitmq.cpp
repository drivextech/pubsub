// Copyright 2020 DriveX.Tech. All rights reserved.
// 
// Licensed under the License.

#include <assert.h>
#include <stdio.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include "utils.h"
#include "pubsub/pubsub_backend_rabbitmq.h"


using std::unique_ptr;


struct PubSubBackendRabbitmq::rabbitmq_internal_data {
    amqp_socket_t* socket;
    string exchange;
    string vhost;
    amqp_channel_t channel;
    amqp_connection_state_t connection;

    rabbitmq_internal_data():
        socket(NULL),
        vhost("vhost"),
        exchange("EX_UWB"),
        channel(4444)
    {}

    ~rabbitmq_internal_data() {}
};


PubSubBackendRabbitmq::PubSubBackendRabbitmq(const string& host_name, int port, const string& pub_name, const string& sub_name):
    PubSubBackend(pub_name, sub_name),
    m_host_name(host_name),
    m_port(port)
{
    m_rabbitmq_internal_data = nullptr;
}

PubSubBackendRabbitmq::~PubSubBackendRabbitmq()
{
    if(m_rabbitmq_internal_data) {
        disconnect();
    }
}

bool PubSubBackendRabbitmq::connect()
{
    m_rabbitmq_internal_data = unique_ptr<rabbitmq_internal_data>(new rabbitmq_internal_data());

    m_rabbitmq_internal_data->connection = amqp_new_connection();

    m_rabbitmq_internal_data->socket = amqp_tcp_socket_new(m_rabbitmq_internal_data->connection);
    if (!m_rabbitmq_internal_data->socket) {
        die("(%s): Creating TCP socket failed!", __func__);
    }

    int status = amqp_socket_open(m_rabbitmq_internal_data->socket, m_host_name.c_str(), m_port);
    if (status) {
        die("(%s): Opening TCP socket failed!", __func__);
    }

    amqp_rpc_reply_t reply = amqp_login(m_rabbitmq_internal_data->connection,
                                        m_rabbitmq_internal_data->vhost.c_str(), 0, AMQP_DEFAULT_FRAME_SIZE, 0, AMQP_SASL_METHOD_PLAIN, "rabbitmq", "rabbitmq");
    die_on_amqp_error(reply, "Logging in failed!");
    amqp_channel_open(m_rabbitmq_internal_data->connection, m_rabbitmq_internal_data->channel);
    die_on_amqp_error(amqp_get_rpc_reply(m_rabbitmq_internal_data->connection), "Opening channel failed!");

    amqp_exchange_declare(m_rabbitmq_internal_data->connection,
                            m_rabbitmq_internal_data->channel,
                            amqp_cstring_bytes(m_rabbitmq_internal_data->exchange.c_str()),
                            amqp_cstring_bytes("direct"), 0, 0, 0, 0,
                            AMQP_EMPTY_TABLE);
    die_on_amqp_error(amqp_get_rpc_reply(m_rabbitmq_internal_data->connection), "Declaring exchange failed!");

    return true;
}

bool PubSubBackendRabbitmq::disconnect()
{
    die_on_amqp_error(amqp_channel_close(m_rabbitmq_internal_data->connection, m_rabbitmq_internal_data->channel, AMQP_REPLY_SUCCESS), "Closing channel");
    die_on_amqp_error(amqp_connection_close(m_rabbitmq_internal_data->connection, AMQP_REPLY_SUCCESS), "Closing connection");
    die_on_error(amqp_destroy_connection(m_rabbitmq_internal_data->connection), "Ending connection");

    m_rabbitmq_internal_data = nullptr;

    return true;
}

bool PubSubBackendRabbitmq::is_connected()
{
    return m_rabbitmq_internal_data != nullptr;
}

bool PubSubBackendRabbitmq::start_publish()
{
    if(!m_rabbitmq_internal_data) {
        return false;
    }

    return true;
}

bool PubSubBackendRabbitmq::start_consume()
{
    if(!m_rabbitmq_internal_data) {
        return false;
    }

    amqp_queue_declare_ok_t* r = amqp_queue_declare(
        m_rabbitmq_internal_data->connection, m_rabbitmq_internal_data->channel, AMQP_EMPTY_BYTES, 0, 0, 0, 1, AMQP_EMPTY_TABLE);
    die_on_amqp_error(amqp_get_rpc_reply(m_rabbitmq_internal_data->connection), "Declaring queue failed!");
    amqp_bytes_t queuename = amqp_bytes_malloc_dup(r->queue);

    amqp_queue_bind(m_rabbitmq_internal_data->connection, m_rabbitmq_internal_data->channel, queuename,
                    amqp_cstring_bytes(m_rabbitmq_internal_data->exchange.c_str()), amqp_cstring_bytes(m_sub_name.c_str()),
                    AMQP_EMPTY_TABLE);
    die_on_amqp_error(amqp_get_rpc_reply(m_rabbitmq_internal_data->connection), "Binding failed!");

    amqp_basic_consume(m_rabbitmq_internal_data->connection, m_rabbitmq_internal_data->channel, queuename, AMQP_EMPTY_BYTES, 0, 1, 0,
                               AMQP_EMPTY_TABLE);
    die_on_amqp_error(amqp_get_rpc_reply(m_rabbitmq_internal_data->connection), "Consuming failed!");

    return true;
}

bool PubSubBackendRabbitmq::publish_data(const uint8_t* data, uint16_t data_len)
{
    amqp_bytes_t message_bytes;

    message_bytes.bytes = (void*)data;
    message_bytes.len = data_len;

    int ret = amqp_basic_publish(m_rabbitmq_internal_data->connection,
                                    m_rabbitmq_internal_data->channel,
                                    amqp_cstring_bytes(m_rabbitmq_internal_data->exchange.c_str()),
                                    amqp_cstring_bytes(m_pub_name.c_str()), 0, 0, NULL,
                                    message_bytes);

    return ret == AMQP_STATUS_OK;
}

void PubSubBackendRabbitmq::run_forever()
{
    int ret_code;
    amqp_rpc_reply_t ret_reply;
    amqp_frame_t frame;

    while(1) {
        amqp_maybe_release_buffers(m_rabbitmq_internal_data->connection);

#if 1
        amqp_envelope_t envelope;
        amqp_message_t message;

        ret_reply = amqp_consume_message(m_rabbitmq_internal_data->connection, &envelope, NULL, 0);

        if(AMQP_RESPONSE_NORMAL != ret_reply.reply_type) {
            // if(AMQP_RESPONSE_LIBRARY_EXCEPTION == ret_reply.reply_type && AMQP_STATUS_UNEXPECTED_FRAME == ret_reply.library_error) {
            if(AMQP_RESPONSE_LIBRARY_EXCEPTION == ret_reply.reply_type && AMQP_STATUS_UNEXPECTED_STATE == ret_reply.library_error) {
                ret_reply = amqp_read_message(m_rabbitmq_internal_data->connection, m_rabbitmq_internal_data->channel, &message, 0);
                printf("(%s): reply type: %u, error: %s, id: %u, frame: %hu\n", __func__,
                        ret_reply.reply_type, amqp_error_string2(ret_reply.library_error), ret_reply.reply.id, frame.frame_type);
                // ret_code = amqp_simple_wait_frame(m_rabbitmq_internal_data->connection, &frame);
                // printf("(%s): reply type: %u, error: %s, id: %u, frame: %hu\n", __func__,
                //         ret_reply.reply_type, amqp_error_string2(ret_reply.library_error), ret_reply.reply.id, frame.frame_type);
            }
            continue;
        }

        if(m_on_data_cber) {
            m_on_data_cber((BYTE*)envelope.message.body.bytes, (WORD)envelope.message.body.len);
        }

        amqp_destroy_envelope(&envelope);

#else
        static uint64_t body_len = 0, body_received = 0;
        static amqp_bytes_t receive_buf;

        ret_code = amqp_simple_wait_frame(m_rabbitmq_internal_data->connection, &frame);

        if(ret_code != AMQP_STATUS_OK) {
            // frame error
            die("(%s): amqp_simple_wait_frame unknown error: %d!", __func__, ret_code);
            continue;
        }

        // printf("(%s): Frame type: %u, id: %#x, channel: %u\n", __func__, frame.frame_type, frame.payload.method.id, frame.channel);

        switch(frame.frame_type) {
            case AMQP_FRAME_METHOD:
                do {
                    switch(frame.payload.method.id) {
                        case AMQP_BASIC_DELIVER_METHOD:
                            do {
                                amqp_basic_deliver_t* delivered_data = (amqp_basic_deliver_t*)frame.payload.method.decoded;
                                printf("(%s): Delivery: %u, exchange: %.*s, routingkey: %.*s\n",
                                    __func__,
                                    (unsigned)delivered_data->delivery_tag, (int)delivered_data->exchange.len,
                                    (char *)delivered_data->exchange.bytes, (int)delivered_data->routing_key.len,
                                    (char *)delivered_data->routing_key.bytes);
                            } while(0);
                            break;
                        case AMQP_BASIC_ACK_METHOD:
                        /* if we've turned publisher confirms on, and we've published a
                        * message here is a message being confirmed.
                        */
                            break;
                        case AMQP_BASIC_RETURN_METHOD:
                        /* if a published message couldn't be routed and the mandatory
                        * flag was set this is what would be returned. The message then
                        * needs to be read.
                        */
                            do {
                                amqp_message_t message;
                                ret_reply = amqp_read_message(m_rabbitmq_internal_data->connection, frame.channel, &message, 0);
                                if(AMQP_RESPONSE_NORMAL != ret_reply.reply_type) {
                                    return;
                                }
                                amqp_destroy_message(&message);
                            } while(0);
                            break;
                        case AMQP_CHANNEL_CLOSE_METHOD:
                            /* a channel.close method happens when a channel exception occurs,
                            * this can happen by publishing to an exchange that doesn't exist
                            * for example.
                            *
                            * In this case you would need to open another channel redeclare
                            * any queues that were declared auto-delete, and restart any
                            * consumers that were attached to the previous channel.
                            */
                            die("(%s): Channel CLOSE Exception ocurred!", __func__);
                            return;
                        default:
                            die("(%s): Unknown Exception ocurred!", __func__);
                            break;
                    }
                } while(0);
                break;
            
            case AMQP_FRAME_BODY:
                do {
                    assert(body_received > 0 && body_len > 0 && receive_buf.len == body_len);

                    if(body_received + frame.payload.body_fragment.len > body_len) {
                        die("(%s): incorrect received message length: %d, expected: %d!", __func__, body_received, body_len);
                    }
                    assert(body_received + frame.payload.body_fragment.len <= body_len);
                    
                    memcpy((char*)receive_buf.bytes + body_received, frame.payload.body_fragment.bytes, frame.payload.body_fragment.len);
                    body_received += frame.payload.body_fragment.len;
                    if(body_received == body_len) {
                        if(m_on_data_cber) {
                            m_on_data_cber((BYTE*)receive_buf.bytes, receive_buf.len);
                        }
                        amqp_bytes_free(receive_buf);
                    }
                } while(0);
                break;

            case AMQP_FRAME_HEADER:
                do {
                    body_received = 0;
                    body_len = frame.payload.properties.body_size;

                    receive_buf = amqp_bytes_malloc(body_len);
                    if(receive_buf.bytes == NULL) {
                        die("(%s): amqp_bytes_malloc failed!", __func__);
                    }
                } while(0);
                break;
                
            default:
                break;
        }

#endif
    }
}
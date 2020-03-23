// Copyright 2020 DriveX.Tech. All rights reserved.
// 
// Licensed under the License.

#pragma once
#ifndef _PUBSUB_BACKEND_RABBITMQ_H_
#define _PUBSUB_BACKEND_RABBITMQ_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory>

#include "pubsub_backend.h"


class PubSubBackendRabbitmq: public PubSubBackend {
public:
    PubSubBackendRabbitmq(const string& host_name, int port = 5672, const string& pub_name = "", const string& sub_name = "");
    virtual ~PubSubBackendRabbitmq();

    virtual bool connect() override;
    virtual bool disconnect() override;
    virtual bool is_connected() override;
    virtual bool start_publish() override;
    virtual bool start_consume() override;

    virtual bool publish_data(const BYTE* data, WORD data_len) override;

    virtual void run_forever() override;

private:
    struct rabbitmq_internal_data;
    std::unique_ptr<rabbitmq_internal_data> m_rabbitmq_internal_data;
    string m_host_name;
    int m_port;
};


#endif

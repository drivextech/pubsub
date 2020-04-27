// Copyright 2020 DriveX.Tech. All rights reserved.
// 
// Licensed under the License.

#pragma once
#ifndef _PUBSUB_BACKEND_H_
#define _PUBSUB_BACKEND_H_

#include <string>
#include <functional>
#include "types/types.h"

using std::string;
using std::function;


class PubSubBackend
{
public:
    typedef void on_data_cber(const dxt_common::BYTE* data, dxt_common::WORD data_len);

    PubSubBackend(const string& pub_name, const string& sub_name);
    virtual ~PubSubBackend();

    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool is_connected() = 0;
    virtual bool start_publish() = 0;
    virtual bool start_consume() = 0;

    virtual bool publish_data(const dxt_common::BYTE* data, dxt_common::WORD data_len) = 0;

    virtual void run_forever() = 0;

    function<PubSubBackend::on_data_cber> m_on_data_cber;

protected:
    string m_pub_name, m_sub_name;
};


#endif
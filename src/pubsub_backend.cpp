// Copyright 2020 DriveX.Tech. All rights reserved.
// 
// Licensed under the License.

#include "pubsub/pubsub_backend.h"


PubSubBackend::PubSubBackend(const string& pub_name, const string& sub_name):
    m_pub_name(pub_name),
    m_sub_name(sub_name)
{
}

PubSubBackend::~PubSubBackend()
{
}
/*
** Copyright 2022 Intel Corporation
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/



#ifndef SOCK_UTILS__H
#define SOCK_UTILS__H

#define MAX_CLIENTS 255

#include <log/log.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <memory>
#include <thread>

#include "SockServer.h"

typedef enum _sock_work_mode {
    SOCK_BLOCK_MODE = -1,
    SOCK_NONBLOCK_MODE = 0,
    SOCK_TIMEOUT_MODE = 1
} sock_work_mode_t;

class SockServer {
public:
    typedef std::function<void(SockServer* sock, sock_client_proxy_t* client)> listener_callback_t;
    typedef std::function<void(SockServer* sock, sock_client_proxy_t* client)> connected_callback_t;
    typedef std::function<void(SockServer* sock, sock_client_proxy_t* client)> disconnect_callback_t;

    SockServer(int port, int sockType);
    ~SockServer();
    static SockServer *getInstance(connected_callback_t conn, listener_callback_t listn);
    int start();
    void stop();
    void join();
    int send_data(const sock_client_proxy_t* client,  const void* data, int len, sock_work_mode_t mode = SOCK_TIMEOUT_MODE, int timeout_ms = 5);
    int send_data_default(const void* data, int len, sock_work_mode_t mode = SOCK_TIMEOUT_MODE, int timeout_ms = 5);
    int recv_data(const sock_client_proxy_t* client, void* data, int len, sock_work_mode_t mode = SOCK_TIMEOUT_MODE, int timeout_ms = 5);
    void register_listener_callback(listener_callback_t func);
    void register_connected_callback(connected_callback_t func);
    void register_disconnected_callback(disconnect_callback_t func);
    int sendConfigMsg(SockServer *server, const void* data, int len, sock_client_proxy_t *client = NULL);
    sock_conn_status_t check_connection_default();
    char* get_ip() { return m_ip; };
    int get_port() { return m_port; };
    sock_client_proxy_t* get_sock_client();
    sock_client_proxy_t** get_connected_clients(){ return m_clients; };
    void set_connection_check_timeout(int timeout_ms){ m_connection_timeout_ms = timeout_ms; }
    void set_client_status_check_timeouts(int timeout_ms) {
        m_client_status_check_timeout = timeout_ms;
    }

private:
    sock_server_t* m_server= nullptr;
    sock_client_proxy_t* m_clients[MAX_CLIENTS] = {nullptr};
    std::mutex m_pclient_mutex_;
    sock_client_proxy_t* m_pclient_ = nullptr;
    char* m_ip = NULL;
    int m_port = 7777;
    int m_ncount = 0;
    std::atomic<bool> m_be_working{false};
    int m_client_status_check_timeout = 8;
    int m_connection_timeout_ms = 1;

    std::unique_ptr<std::thread>    m_thread = nullptr;
    listener_callback_t m_listener_callback = nullptr;
    connected_callback_t m_connected_callback = nullptr;
    disconnect_callback_t m_disconnected_callback = nullptr;

private:
    SockServer(const SockServer &cg_server);
    SockServer& operator=(const SockServer&){ return *this; }
    static void task(SockServer * ptr) {
        ptr->listener();
    }
    void listener();
    int check_new_connection();
    int32_t check_new_message();
    int m_sock_type;
};

#endif // CG_SOCK_H

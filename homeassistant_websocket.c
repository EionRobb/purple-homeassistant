#include "homeassistant_websocket.h"
#include "homeassistant_connection.h"
#include <json-glib/json-glib.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

static void ha_websocket_got_data(gpointer userdata, gint source, PurpleInputCondition cond);
static void ha_websocket_got_ssl_data(gpointer userdata, PurpleSslConnection *conn, PurpleInputCondition cond);

static gssize
ha_ws_write(HAAccount *ha, const guchar *buf, gsize len)
{
    if (ha->is_ssl && ha->websocket) {
        return purple_ssl_write(ha->websocket, buf, len);
    } else if (!ha->is_ssl && ha->websocket_fd > 0) {
#ifdef _WIN32
        return send(ha->websocket_fd, (const char *)buf, len, 0);
#else
        return write(ha->websocket_fd, buf, len);
#endif
    }
    return -1;
}

static gssize
ha_ws_read(HAAccount *ha, guchar *buf, gsize len)
{
    if (ha->is_ssl && ha->websocket) {
        return purple_ssl_read(ha->websocket, buf, len);
    } else if (!ha->is_ssl && ha->websocket_fd > 0) {
#ifdef _WIN32
        return recv(ha->websocket_fd, (char *)buf, len, 0);
#else
        return read(ha->websocket_fd, buf, len);
#endif
    }
    return -1;
}

static guchar *
ha_websocket_mask(const guchar key[4], const guchar *pload, guint64 psize)
{
    guchar *masked = g_new0(guchar, psize);
    guint64 i;
    for (i = 0; i < psize; i++) {
        masked[i] = pload[i] ^ key[i % 4];
    }
    return masked;
}

static void
ha_websocket_write_data(HAAccount *ha, const guchar *data, gsize data_len, guchar type)
{
    guchar *full_data;
    guint len_size = 1;
    guchar mkey[4] = { 0x12, 0x34, 0x56, 0x78 }; // Random masking key
    int ret;

    guchar *masked_data = ha_websocket_mask(mkey, data, data_len);

    if (data_len > 125) {
        if (data_len <= G_MAXUINT16) {
            len_size += 2;
        } else {
            len_size += 8;
        }
    }

    full_data = g_new0(guchar, 1 + data_len + len_size + 4);

    if (type == 0) {
        type = 129; // Text frame
    }

    full_data[0] = type;

    if (data_len <= 125) {
        full_data[1] = data_len | 0x80;
    } else if (data_len <= G_MAXUINT16) {
        guint16 be_len = GUINT16_TO_BE(data_len);
        full_data[1] = 126 | 0x80;
        memmove(full_data + 2, &be_len, 2);
    } else {
        guint64 be_len = GUINT64_TO_BE(data_len);
        full_data[1] = 127 | 0x80;
        memmove(full_data + 2, &be_len, 8);
    }

    memmove(full_data + (1 + len_size), &mkey, 4);
    memmove(full_data + (1 + len_size + 4), masked_data, data_len);

    do {
        ret = ha_ws_write(ha, full_data, 1 + data_len + len_size + 4);
        if (ret < 0 && errno != EAGAIN) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                errno = EAGAIN;
                continue;
            }
#endif
            purple_debug_error("homeassistant", "websocket sending error\n");
            purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Websocket failed to send");
            break;
        }
    } while(ret < 0 && errno == EAGAIN);

    g_free(full_data);
    g_free(masked_data);
}

static void
ha_websocket_write_json(HAAccount *ha, JsonObject *data)
{
    JsonNode *node;
    gchar *str;
    gsize len;
    JsonGenerator *generator;

    node = json_node_new(JSON_NODE_OBJECT);
    json_node_set_object(node, data);

    generator = json_generator_new();
    json_generator_set_root(generator, node);
    str = json_generator_to_data(generator, &len);
    g_object_unref(generator);
    json_node_free(node);

    purple_debug_info("homeassistant", "sending frame: %s\n", str);
    ha_websocket_write_data(ha, (const guchar *) str, len, 0);

    g_free(str);
}

static void
ha_websocket_send_auth(HAAccount *ha)
{
    JsonObject *data = json_object_new();
    json_object_set_string_member(data, "type", "auth");
    json_object_set_string_member(data, "access_token", ha->api_key);
    ha_websocket_write_json(ha, data);
}

static void
ha_websocket_send_subscribe(HAAccount *ha)
{
    ha->message_id++;
    JsonObject *data = json_object_new();
    json_object_set_int_member(data, "id", ha->message_id);
    json_object_set_string_member(data, "type", "subscribe_events");
    json_object_set_string_member(data, "event_type", "state_changed");
    ha_websocket_write_json(ha, data);
}

static void
ha_websocket_get_states(HAAccount *ha)
{
    ha->message_id++;
    ha->get_states_msg_id = ha->message_id;
    JsonObject *data = json_object_new();
    json_object_set_int_member(data, "id", ha->message_id);
    json_object_set_string_member(data, "type", "get_states");
    ha_websocket_write_json(ha, data);
}

static void
ha_websocket_get_areas(HAAccount *ha)
{
    ha->message_id++;
    ha->get_areas_msg_id = ha->message_id;
    JsonObject *data = json_object_new();
    json_object_set_int_member(data, "id", ha->message_id);
    json_object_set_string_member(data, "type", "config/area_registry/list");
    ha_websocket_write_json(ha, data);
}

static void
ha_websocket_get_entities(HAAccount *ha)
{
    ha->message_id++;
    ha->get_entities_msg_id = ha->message_id;
    JsonObject *data = json_object_new();
    json_object_set_int_member(data, "id", ha->message_id);
    json_object_set_string_member(data, "type", "config/entity_registry/list");
    ha_websocket_write_json(ha, data);
}

void
ha_websocket_call_service_with_data(HAAccount *ha, const gchar *domain, const gchar *service, JsonObject *service_data)
{
    ha->message_id++;
    JsonObject *data = json_object_new();
    json_object_set_int_member(data, "id", ha->message_id);
    json_object_set_string_member(data, "type", "call_service");
    json_object_set_string_member(data, "domain", domain);
    json_object_set_string_member(data, "service", service);
    
    if (service_data) {
        json_object_set_object_member(data, "service_data", service_data);
    }
    
    ha_websocket_write_json(ha, data);
}

void
ha_websocket_call_service(HAAccount *ha, const gchar *domain, const gchar *service, const gchar *entity_id)
{
    JsonObject *service_data = json_object_new();
    json_object_set_string_member(service_data, "entity_id", entity_id);
    ha_websocket_call_service_with_data(ha, domain, service, service_data);
}

static void
ha_websocket_process_frame(HAAccount *ha, const gchar *frame)
{
    JsonParser *parser = json_parser_new();
    GError *error = NULL;

    purple_debug_info("homeassistant", "received frame: %s\n", frame);

    if (json_parser_load_from_data(parser, frame, -1, &error)) {
        JsonNode *root = json_parser_get_root(parser);
        if (json_node_get_node_type(root) == JSON_NODE_OBJECT) {
            JsonObject *obj = json_node_get_object(root);
            const gchar *type = json_object_get_string_member(obj, "type");
            
            if (g_strcmp0(type, "auth_required") == 0) {
                ha_websocket_send_auth(ha);
            } else if (g_strcmp0(type, "auth_ok") == 0) {
                purple_connection_set_state(ha->pc, PURPLE_CONNECTED);
                ha_websocket_send_subscribe(ha);
                ha_websocket_get_areas(ha);
                ha_websocket_get_entities(ha);
                ha_websocket_get_states(ha);
            } else if (g_strcmp0(type, "auth_invalid") == 0) {
                purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, "Authentication Failed");
            } else if (g_strcmp0(type, "event") == 0) {
                JsonObject *event = json_object_has_member(obj, "event") ? json_object_get_object_member(obj, "event") : NULL;
                if (event && json_object_has_member(event, "data")) {
                    JsonObject *data = json_object_get_object_member(event, "data");
                    if (json_object_has_member(data, "new_state")) {
                        JsonObject *new_state = json_object_get_object_member(data, "new_state");
                        if (new_state) {
                            ha_process_state_change_event(ha, new_state);
                        }
                    }
                }
            } else if (g_strcmp0(type, "result") == 0) {
                if (json_object_has_member(obj, "success") && json_object_get_boolean_member(obj, "success")) {
                    guint id = json_object_has_member(obj, "id") ? json_object_get_int_member(obj, "id") : 0;
                    JsonNode *res_node = json_object_has_member(obj, "result") ? json_object_get_member(obj, "result") : NULL;
                    
                    if (res_node) {
                        if (id == ha->get_areas_msg_id) {
                            if (json_node_get_node_type(res_node) == JSON_NODE_ARRAY) {
                                JsonArray *arr = json_node_get_array(res_node);
                                guint i, len = json_array_get_length(arr);
                                for (i = 0; i < len; i++) {
                                    JsonObject *area = json_array_get_object_element(arr, i);
                                    const gchar *area_id = json_object_get_string_member(area, "area_id");
                                    const gchar *name = json_object_get_string_member(area, "name");
                                    if (area_id && name) {
                                        g_hash_table_insert(ha->areas, g_strdup(area_id), g_strdup(name));
                                    }
                                }
                            }
                        } else if (id == ha->get_entities_msg_id) {
                            if (json_node_get_node_type(res_node) == JSON_NODE_ARRAY) {
                                JsonArray *arr = json_node_get_array(res_node);
                                guint i, len = json_array_get_length(arr);
                                for (i = 0; i < len; i++) {
                                    JsonObject *entity = json_array_get_object_element(arr, i);
                                    const gchar *entity_id = json_object_get_string_member(entity, "entity_id");
                                    const gchar *area_id = json_object_has_member(entity, "area_id") ? json_object_get_string_member(entity, "area_id") : NULL;
                                    if (entity_id && area_id) {
                                        g_hash_table_insert(ha->entity_areas, g_strdup(entity_id), g_strdup(area_id));
                                    }
                                }
                            }
                        } else if (id == ha->get_states_msg_id) {
                            if (json_node_get_node_type(res_node) == JSON_NODE_ARRAY) {
                                ha_process_entities(ha, json_node_get_array(res_node));
                            }
                        }
                    }
                }
            }
        }
    } else {
        purple_debug_error("homeassistant", "Failed to parse websocket JSON: %s\n", error->message);
        g_error_free(error);
    }
    g_object_unref(parser);
}

static void
ha_websocket_read_loop(HAAccount *ha)
{
    guchar length_code;
    int read_len = 0;
    gboolean done_some_reads = FALSE;

    if (G_UNLIKELY(!ha->websocket_header_received)) {
        gint nlbr_count = 0;
        gchar nextchar;

        while (nlbr_count < 4 && ha_ws_read(ha, (guchar *)&nextchar, 1) == 1) {
            if (nextchar == '\r' || nextchar == '\n') {
                nlbr_count++;
            } else {
                nlbr_count = 0;
            }
        }

        if (nlbr_count == 4) {
            ha->websocket_header_received = TRUE;
            done_some_reads = TRUE;
        }
    }

    while (ha->frame || (read_len = ha_ws_read(ha, &ha->packet_code, 1)) == 1) {
        if (!ha->frame) {
            if (ha->packet_code != 129 && ha->packet_code != 130) {
                if (ha->packet_code == 136) { // Close
                    purple_debug_error("homeassistant", "websocket closed by server\n");
                    ha_websocket_connect(ha); // Attempt reconnect
                    return;
                } else if (ha->packet_code == 137) { // Ping
                    gint ping_frame_len = 0;
                    length_code = 0;
                    ha_ws_read(ha, &length_code, 1);
                    if (length_code <= 125) {
                        ping_frame_len = length_code;
                    }
                    if (ping_frame_len > 0) {
                        guchar *pong_data = g_new0(guchar, ping_frame_len);
                        ha_ws_read(ha, pong_data, ping_frame_len);
                        ha_websocket_write_data(ha, pong_data, ping_frame_len, 138); // Pong
                        g_free(pong_data);
                    } else {
                        ha_websocket_write_data(ha, (const guchar *) "", 0, 138);
                    }
                    return;
                } else if (ha->packet_code == 138) { // Pong
                    return;
                }
                return;
            }

            length_code = 0;
            ha_ws_read(ha, &length_code, 1);
            length_code = length_code & ~0x80; // mask bit should be 0 from server, but clear it just in case

            if (length_code <= 125) {
                ha->frame_len = length_code;
            } else if (length_code == 126) {
                ha_ws_read(ha, (guchar *)&ha->frame_len, 2);
                ha->frame_len = GUINT16_FROM_BE(ha->frame_len);
            } else if (length_code == 127) {
                ha_ws_read(ha, (guchar *)&ha->frame_len, 8);
                ha->frame_len = GUINT64_FROM_BE(ha->frame_len);
            }

            if (ha->frame_len > (16 * 1024 * 1024)) {
                purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Websocket protocol error: unreasonable frame length");
                return;
            }

            ha->frame = g_new0(gchar, ha->frame_len + 1);
            ha->frame_len_progress = 0;
        }

        do {
            read_len = ha_ws_read(ha, (guchar *)(ha->frame + ha->frame_len_progress), ha->frame_len - ha->frame_len_progress);

            if (read_len > 0) {
                ha->frame_len_progress += read_len;
            }
        } while (read_len > 0 && ha->frame_len_progress < ha->frame_len);

        done_some_reads = TRUE;

        if (ha->frame_len_progress == ha->frame_len) {
            ha_websocket_process_frame(ha, ha->frame);
            g_free(ha->frame);
            ha->frame = NULL;
            ha->packet_code = 0;
            ha->frame_len = 0;
            ha->frames_since_reconnect++;
        } else {
            return;
        }
    }

    if (done_some_reads == FALSE && read_len <= 0) {
#ifdef _WIN32
        if (read_len < 0 && (errno == EAGAIN || WSAGetLastError() == WSAEWOULDBLOCK)) {
            return;
        }
#else
        if (read_len < 0 && errno == EAGAIN) {
            return;
        }
#endif
        if (ha->frames_since_reconnect < 2) {
            purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Lost connection to server");
        } else {
            ha_websocket_connect(ha);
        }
    }
}

static void
ha_websocket_got_ssl_data(gpointer userdata, PurpleSslConnection *conn, PurpleInputCondition cond)
{
    HAAccount *ha = userdata;
    if (conn != ha->websocket) return;
    ha_websocket_read_loop(ha);
}

static void
ha_websocket_got_data(gpointer userdata, gint source, PurpleInputCondition cond)
{
    HAAccount *ha = userdata;
    if (source != ha->websocket_fd) return;
    ha_websocket_read_loop(ha);
}

static void
ha_websocket_connected_impl(HAAccount *ha)
{
    gchar *websocket_header;
    const gchar *websocket_key = "15XF+ptKDhYVERXoGcdHTA=="; // standard dummy key

    gchar *host = NULL;
    if (g_str_has_prefix(ha->server_url, "https://")) {
        host = g_strdup(ha->server_url + 8);
    } else if (g_str_has_prefix(ha->server_url, "http://")) {
        host = g_strdup(ha->server_url + 7);
    } else {
        host = g_strdup(ha->server_url);
    }

    if (ha->is_ssl) {
        purple_ssl_input_add(ha->websocket, ha_websocket_got_ssl_data, ha);
    } else {
        ha->websocket_inpa = purple_input_add(ha->websocket_fd, PURPLE_INPUT_READ, ha_websocket_got_data, ha);
    }

    websocket_header = g_strdup_printf(
        "GET /api/websocket HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "\r\n",
        host, websocket_key
    );

    ha_ws_write(ha, (const guchar *)websocket_header, strlen(websocket_header));

    g_free(websocket_header);
    g_free(host);
}

static void
ha_websocket_ssl_connected(gpointer userdata, PurpleSslConnection *conn, PurpleInputCondition cond)
{
    HAAccount *ha = userdata;
    ha_websocket_connected_impl(ha);
}

static void
ha_websocket_tcp_connected(gpointer userdata, gint source, const gchar *error_message)
{
    HAAccount *ha = userdata;
    ha->websocket_conn_data = NULL;
    
    if (source < 0) {
        purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Couldn't connect to WebSocket API");
        return;
    }
    
    ha->websocket_fd = source;
    ha_websocket_connected_impl(ha);
}

static void
ha_websocket_failed(PurpleSslConnection *conn, PurpleSslErrorType errortype, gpointer userdata)
{
    HAAccount *ha = userdata;
    ha->websocket = NULL;
    ha->websocket_header_received = FALSE;
    purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Couldn't connect to WebSocket API");
}

void
ha_websocket_close(HAAccount *ha)
{
    if (ha->websocket != NULL) {
        purple_ssl_close(ha->websocket);
        ha->websocket = NULL;
    }
    if (ha->websocket_inpa) {
        purple_input_remove(ha->websocket_inpa);
        ha->websocket_inpa = 0;
    }
    if (ha->websocket_conn_data) {
        purple_proxy_connect_cancel(ha->websocket_conn_data);
        ha->websocket_conn_data = NULL;
    }
    if (ha->websocket_fd > 0) {
#ifdef _WIN32
        closesocket(ha->websocket_fd);
#else
        close(ha->websocket_fd);
#endif
        ha->websocket_fd = 0;
    }
    g_free(ha->frame);
    ha->frame = NULL;
    ha->websocket_header_received = FALSE;
}

void
ha_websocket_connect(HAAccount *ha)
{
    ha_websocket_close(ha);

    gchar *host = NULL;
    int port = 8123;
    ha->is_ssl = FALSE;
    
    const gchar *url_part = ha->server_url;
    if (g_str_has_prefix(url_part, "https://")) {
        url_part += 8;
        port = 443;
        ha->is_ssl = TRUE;
    } else if (g_str_has_prefix(url_part, "http://")) {
        url_part += 7;
        port = 80;
        ha->is_ssl = FALSE;
    }
    
    gchar **host_parts = g_strsplit(url_part, ":", 2);
    host = g_strdup(host_parts[0]);
    if (host_parts[1]) {
        gchar **port_parts = g_strsplit(host_parts[1], "/", 2);
        port = atoi(port_parts[0]);
        g_strfreev(port_parts);
    }
    g_strfreev(host_parts);

    ha->frames_since_reconnect = 0;
    ha->message_id = 1;

    if (ha->is_ssl) {
        ha->websocket = purple_ssl_connect(ha->account, host, port, ha_websocket_ssl_connected, ha_websocket_failed, ha);
    } else {
        ha->websocket_conn_data = purple_proxy_connect(ha->pc, ha->account, host, port, ha_websocket_tcp_connected, ha);
    }
    
    g_free(host);
}

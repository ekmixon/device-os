/* Automatically generated nanopb header */
/* Generated by nanopb-0.3.9.3 at Thu Dec 30 21:55:50 2021. */

#ifndef PB_PARTICLE_CTRL_CLOUD_CLOUD_PB_H_INCLUDED
#define PB_PARTICLE_CTRL_CLOUD_CLOUD_PB_H_INCLUDED
#include <pb.h>

#include "extensions.pb.h"

/* @@protoc_insertion_point(includes) */
#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
typedef enum _particle_ctrl_cloud_ConnectionStatus {
    particle_ctrl_cloud_ConnectionStatus_DISCONNECTED = 0,
    particle_ctrl_cloud_ConnectionStatus_CONNECTING = 1,
    particle_ctrl_cloud_ConnectionStatus_CONNECTED = 2,
    particle_ctrl_cloud_ConnectionStatus_DISCONNECTING = 3
} particle_ctrl_cloud_ConnectionStatus;
#define _particle_ctrl_cloud_ConnectionStatus_MIN particle_ctrl_cloud_ConnectionStatus_DISCONNECTED
#define _particle_ctrl_cloud_ConnectionStatus_MAX particle_ctrl_cloud_ConnectionStatus_DISCONNECTING
#define _particle_ctrl_cloud_ConnectionStatus_ARRAYSIZE ((particle_ctrl_cloud_ConnectionStatus)(particle_ctrl_cloud_ConnectionStatus_DISCONNECTING+1))

/* Struct definitions */
typedef struct _particle_ctrl_cloud_ConnectReply {
    char dummy_field;
/* @@protoc_insertion_point(struct:particle_ctrl_cloud_ConnectReply) */
} particle_ctrl_cloud_ConnectReply;

typedef struct _particle_ctrl_cloud_ConnectRequest {
    char dummy_field;
/* @@protoc_insertion_point(struct:particle_ctrl_cloud_ConnectRequest) */
} particle_ctrl_cloud_ConnectRequest;

typedef struct _particle_ctrl_cloud_DisconnectReply {
    char dummy_field;
/* @@protoc_insertion_point(struct:particle_ctrl_cloud_DisconnectReply) */
} particle_ctrl_cloud_DisconnectReply;

typedef struct _particle_ctrl_cloud_DisconnectRequest {
    char dummy_field;
/* @@protoc_insertion_point(struct:particle_ctrl_cloud_DisconnectRequest) */
} particle_ctrl_cloud_DisconnectRequest;

typedef struct _particle_ctrl_cloud_GetConnectionStatusRequest {
    char dummy_field;
/* @@protoc_insertion_point(struct:particle_ctrl_cloud_GetConnectionStatusRequest) */
} particle_ctrl_cloud_GetConnectionStatusRequest;

typedef struct _particle_ctrl_cloud_GetConnectionStatusReply {
    particle_ctrl_cloud_ConnectionStatus status;
/* @@protoc_insertion_point(struct:particle_ctrl_cloud_GetConnectionStatusReply) */
} particle_ctrl_cloud_GetConnectionStatusReply;

/* Default values for struct fields */

/* Initializer values for message structs */
#define particle_ctrl_cloud_GetConnectionStatusRequest_init_default {0}
#define particle_ctrl_cloud_GetConnectionStatusReply_init_default {_particle_ctrl_cloud_ConnectionStatus_MIN}
#define particle_ctrl_cloud_ConnectRequest_init_default {0}
#define particle_ctrl_cloud_ConnectReply_init_default {0}
#define particle_ctrl_cloud_DisconnectRequest_init_default {0}
#define particle_ctrl_cloud_DisconnectReply_init_default {0}
#define particle_ctrl_cloud_GetConnectionStatusRequest_init_zero {0}
#define particle_ctrl_cloud_GetConnectionStatusReply_init_zero {_particle_ctrl_cloud_ConnectionStatus_MIN}
#define particle_ctrl_cloud_ConnectRequest_init_zero {0}
#define particle_ctrl_cloud_ConnectReply_init_zero {0}
#define particle_ctrl_cloud_DisconnectRequest_init_zero {0}
#define particle_ctrl_cloud_DisconnectReply_init_zero {0}

/* Field tags (for use in manual encoding/decoding) */
#define particle_ctrl_cloud_GetConnectionStatusReply_status_tag 1

/* Struct field encoding specification for nanopb */
extern const pb_field_t particle_ctrl_cloud_GetConnectionStatusRequest_fields[1];
extern const pb_field_t particle_ctrl_cloud_GetConnectionStatusReply_fields[2];
extern const pb_field_t particle_ctrl_cloud_ConnectRequest_fields[1];
extern const pb_field_t particle_ctrl_cloud_ConnectReply_fields[1];
extern const pb_field_t particle_ctrl_cloud_DisconnectRequest_fields[1];
extern const pb_field_t particle_ctrl_cloud_DisconnectReply_fields[1];

/* Maximum encoded size of messages (where known) */
#define particle_ctrl_cloud_GetConnectionStatusRequest_size 0
#define particle_ctrl_cloud_GetConnectionStatusReply_size 2
#define particle_ctrl_cloud_ConnectRequest_size  0
#define particle_ctrl_cloud_ConnectReply_size    0
#define particle_ctrl_cloud_DisconnectRequest_size 0
#define particle_ctrl_cloud_DisconnectReply_size 0

/* Message IDs (where set with "msgid" option) */
#ifdef PB_MSGID

#define CLOUD_MESSAGES \


#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
/* @@protoc_insertion_point(eof) */

#endif

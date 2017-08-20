/*
 * Copyright (C) 2017 by Daniel Clouse.
 *
 * This file is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 *
 *  a) This library is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of the
 *     License, or (at your option) any later version.
 *
 *     This library is distributed in the hope that it will be useful, 
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this library; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 *     MA 02110-1301 USA
 *
 * Alternatively,
 *
 *  b) Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *     CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *     INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *     MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *     CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *     SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *     NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *     OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *     EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "tcp_comms.h"
#include "get_ip_addr_str.h"
#include "mmal.h"
#include "get_usecs.h"

typedef struct {
    Tcp_Params* params_ptr;
    Tcp_Host_Info* client_ptr;
    MMAL_COMPONENT_T* camera_ptr;
} Connection_Thread_Info;


static inline int_limit(int low, int high, int value) {
    return (value < low) ? low : (value > high) ? high : value;
}

static inline float_limit(float low, float high, float value) {
    return (value < low) ? low : (value > high) ? high : value;
}

static void* connection_thread(void* void_args_ptr) {
#define INT1    8
#define INT2   12
#define INT3   16
#define FLOAT1  8
#define FLOAT2 12
#define FLOAT4 24

#define MAX_CLIENT_STRING 64
    char client_string[MAX_CLIENT_STRING];
#define MAX_MESG 64
    char mesg[MAX_MESG];
    Connection_Thread_Info* args_ptr = (Connection_Thread_Info*)void_args_ptr;
    Tcp_Params* params_ptr = args_ptr->params_ptr;
    Tcp_Host_Info* client_ptr = args_ptr->client_ptr;
    MMAL_COMPONENT_T* camera_ptr = args_ptr->camera_ptr;

    Raspicam_Char_Msg* char_msg_ptr = (Raspicam_Char_Msg*)mesg;
    Raspicam_Int_Msg* int_msg_ptr = (Raspicam_Int_Msg*)mesg;
    Raspicam_Float_Msg* float_msg_ptr = (Raspicam_Float_Msg*)mesg;
    bool error_seen = false;
    ssize_t bytes;
    while ((bytes = recv(client_ptr->fd, &mesg, sizeof(mesg), 0)) > 0) {
        float timestamp = get_usecs() / (float)USECS_PER_SECOND;
        pthread_mutex_lock(&params_ptr->params_mutex);
        switch (mesg[0]) {
        case RASPICAM_QUIT:
            LOG_STATUS("at %.3f, tcp client %s quit\n",
                       get_ip_addr_str(
                                    (const struct sockaddr*)&client_ptr->saddr,
                                    client_string, MAX_CLIENT_STRING));
            free(client_ptr);
            return NULL;
        case RASPICAM_SATURATION:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.saturation =
                               int_limit(-100, 100, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_saturation(
                               camera_ptr, params_ptr->cam_params.saturation);
            }
            break;
        case RASPICAM_SHARPNESS :
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.sharpness =
                               int_limit(-100, 100, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_sharpness(camera_ptr, 
                                              params_ptr->cam_params.sharpness);
            }
            break;
        case RASPICAM_CONTRAST:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.contrast =
                               int_limit(-100, 100, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_contrast(camera_ptr, 
                                             params_ptr->cam_params.contrast);
            }
            break;
        case RASPICAM_BRIGHTNESS:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.brightness =
                                   int_limit(0, 100, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_brightness(
                                camera_ptr, params_ptr->cam_params.brightness);
            }
            break;
        case RASPICAM_ISO:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.ISO = ntohl(int_msg_ptr->int0);
                raspicamcontrol_set_ISO(camera_ptr, params_ptr->cam_params.ISO);
            }
            break;
        case RASPICAM_METERING_MODE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.exposureMeterMode =
                                    (MMAL_PARAM_EXPOSUREMETERINGMODE_T)
                                    int_limit(0, 3, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_metering_mode(
                         camera_ptr, params_ptr->cam_params.exposureMeterMode);
            }
            break;
        case RASPICAM_VIDEO_STABILISATION:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.videoStabilisation =
                                     int_limit(0, 1, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_video_stabilisation(
                        camera_ptr, params_ptr->cam_params.videoStabilisation);
            }
            break;
        case RASPICAM_EXPOSURE_COMPENSATION:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.exposureCompensation =
                                   int_limit(-10, 10, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_exposure_compensation(
                      camera_ptr, params_ptr->cam_params.exposureCompensation);
            }
            break;
        case RASPICAM_EXPOSURE_MODE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.exposureMode =
                                    (MMAL_PARAM_EXPOSUREMODE_T)
                                    int_limit(0, 12, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_exposure_mode(
                              camera_ptr, params_ptr->cam_params.exposureMode);
            }
            break;
        case RASPICAM_AWB_MODE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.awbMode = (MMAL_PARAM_AWBMODE_T)
                                    int_limit(0, 9, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_awb_mode(camera_ptr,
                                             params_ptr->cam_params.awbMode);
            }
            break;
        case RASPICAM_AWB_GAINS:
            if (bytes < FLOAT2) {
                error_seen = true;
            } else {
                params_ptr->cam_params.awb_gains_r =
                        float_limit(0.00001, 1.0, ntohl(float_msg_ptr->float0));
                params_ptr->cam_params.awb_gains_b =
                        float_limit(0.00001, 1.0, ntohl(float_msg_ptr->float1));
                raspicamcontrol_set_awb_gains(
                                           camera_ptr,
                                           params_ptr->cam_params.awb_gains_r,
                                           params_ptr->cam_params.awb_gains_b);
            }
            break;
        case RASPICAM_IMAGE_FX:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.imageEffect = (MMAL_PARAM_IMAGEFX_T)
                                    int_limit(0, 22, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_imageFX(camera_ptr, 
                                            params_ptr->cam_params.imageEffect);
            }
            break;
        case RASPICAM_COLOUR_FX:
            if (bytes < INT3) {
                error_seen = true;
            } else {
                params_ptr->cam_params.colourEffects.enable =
                                    int_limit(0, 1, ntohl(int_msg_ptr->int0));
                params_ptr->cam_params.colourEffects.u =
                                    int_limit(0, 255, ntohl(int_msg_ptr->int1));
                params_ptr->cam_params.colourEffects.v =
                                    int_limit(0, 255, ntohl(int_msg_ptr->int2));
                raspicamcontrol_set_colourFX(
                             camera_ptr, &params_ptr->cam_params.colourEffects);
            }
            break;
        case RASPICAM_ROTATION:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.rotation =
                                    int_limit(0, 359, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_awb_mode(camera_ptr,
                                             params_ptr->cam_params.rotation);
            }
            break;
        case RASPICAM_FLIPS:
            if (bytes < INT2) {
                error_seen = true;
            } else {
                params_ptr->cam_params.hflip =
                                    int_limit(0, 1, ntohl(int_msg_ptr->int0));
                params_ptr->cam_params.vflip =
                                    int_limit(0, 1, ntohl(int_msg_ptr->int1));
                raspicamcontrol_set_flips(camera_ptr,
                                          params_ptr->cam_params.hflip,
                                          params_ptr->cam_params.vflip);
            }
            break;
        case RASPICAM_ROI:
            if (bytes < FLOAT4) {
                error_seen = true;
            } else {
                params_ptr->cam_params.roi.x =
                           float_limit(0.0, 1.0, ntohl(float_msg_ptr->float0));
                params_ptr->cam_params.roi.y =
                           float_limit(0.0, 1.0, ntohl(float_msg_ptr->float1));
                params_ptr->cam_params.roi.w =
                           float_limit(0.0, 1.0, ntohl(float_msg_ptr->float2));
                params_ptr->cam_params.roi.h =
                           float_limit(0.0, 1.0, ntohl(float_msg_ptr->float3));
                raspicamcontrol_set_ROI(camera_ptr, params_ptr->cam_params.roi);
            }
            break;
        case RASPICAM_SHUTTER_SPEED:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.shutter_speed = ntohl(int_msg_ptr->int0);
                raspicamcontrol_set_shutter_speed(
                             camera_ptr, params_ptr->cam_params.shutter_speed);
            }
            break;
        case RASPICAM_DRC:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.drc_level =
                                     (MMAL_PARAMETER_DRC_STRENGTH_T)
                                     int_limit(0, 3, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_DRC(camera_ptr,
                                        params_ptr->cam_params.drc_level);
            }
            break;
        case RASPICAM_STATS_PASS:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->cam_params.stats_pass =
                                     int_limit(0, 1, ntohl(int_msg_ptr->int0));
                raspicamcontrol_set_stats_pass(
                                camera_ptr, params_ptr->cam_params.stats_pass);
            }
            break;
        case RASPICAM_TEST_IMAGE_ENABLE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->test_img_enable =
                                     int_limit(0, 1, ntohl(int_msg_ptr->int0));
            }
            break;
        case RASPICAM_YUV_WRITE_ENABLE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->yuv_write = int_limit(0, 1,
                                                  ntohl(int_msg_ptr->int0));
            }
            break;
        case RASPICAM_JPG_WRITE_ENABLE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->jpg_write = int_limit(0, 1,
                                                  ntohl(int_msg_ptr->int0));
            }
            break;
        case RASPICAM_DETECT_YUV_ENABLE:
            if (bytes < INT1) {
                error_seen = true;
            } else {
                params_ptr->detect_yuv = int_limit(0, 1,
                                                   ntohl(int_msg_ptr->int0));
            }
            break;
        case RASPICAM_BLOB_YUV:
            if (bytes < 7) {
                error_seen = true;
            } else {
                params_ptr->blob_yuv_min[0] = char_msg_ptr->c[0];
                params_ptr->blob_yuv_max[0] = char_msg_ptr->c[1];
                params_ptr->blob_yuv_min[1] = char_msg_ptr->c[2];
                params_ptr->blob_yuv_max[1] = char_msg_ptr->c[3];
                params_ptr->blob_yuv_min[2] = char_msg_ptr->c[4];
                params_ptr->blob_yuv_max[2] = char_msg_ptr->c[5];
            }
            break;
        default:
            LOG_ERROR("at %.3f, unexpected tcp message tag %d from %s\n",
                      timestamp, mesg[0],
                      get_ip_addr_str(
                                  (const struct sockaddr*)&client_ptr->saddr,
                                  client_string, MAX_CLIENT_STRING));
        }
        pthread_mutex_unlock(&params_ptr->params_mutex);
        if (error_seen) {
            LOG_ERROR(
                "at %.3f, too few bytes (%d) in tcp message tag %d from %s\n",
                      timestamp, bytes, mesg[0],
                      get_ip_addr_str(
                                  (const struct sockaddr*)&client_ptr->saddr,
                                  client_string, MAX_CLIENT_STRING));
        }
    }
    LOG_ERROR("at %.3f, can't recv from tcp client %s; errno= %d\n",
              get_usecs() / (float)USECS_PER_SECOND,
              get_ip_addr_str((const struct sockaddr*)&client_ptr->saddr,
                              client_string, MAX_CLIENT_STRING),
              errno);
    free(client_ptr);
    return NULL;
}

/**
 * Start up the message_loop().
 */
static void* server_thread(void* void_args_ptr) {
    Tcp_Comms* comms_ptr = (Tcp_Comms*)void_args_ptr;
    int socket_fd = comms_ptr->server.fd;
    int client_fd;
    struct sockaddr_storage client_addr;

    socklen_t bytes = sizeof(client_addr);

    // We always keep around space for one unused client to work in.

    Tcp_Host_Info* client_ptr = (Tcp_Host_Info*)malloc(sizeof(Tcp_Host_Info));
    while ((client_ptr->fd = accept(socket_fd,
                                    (struct sockaddr*)&client_ptr->saddr,
                                    &client_ptr->saddr_len)) >= 0) {
        pthread_t pthread_id;

        // Start a new thread to handle all communications with the new client.

        int status = pthread_create(&pthread_id, NULL, connection_thread,
                                    client_ptr);
        if (status != 0) {
            LOG_ERROR("at %.3f, tcp_comms can't pthread_create (%d)\n",
                      get_usecs() / (float)USECS_PER_SECOND, status);
            return NULL;
        }

        // Make space for a new (unused) client.

        client_ptr = (Tcp_Host_Info*)malloc(sizeof(Tcp_Host_Info));
    }
    free(client_ptr);
    LOG_ERROR("at %.3f, tcp_comms can't accept; errno=%d\n",
              get_usecs() / (float)USECS_PER_SECOND, errno);
    return NULL;
}

int tcp_params_construct(Tcp_Params* params_ptr) {
    int status = pthread_mutex_init(&params_ptr->params_mutex, NULL);
    if (status != 0) return status;
    raspicamcontrol_set_defaults(&params_ptr->cam_params);
    params_ptr->test_img_enable = false;
    params_ptr->yuv_write = false;
    params_ptr->jpg_write = false;
    params_ptr->detect_yuv = false;
    params_ptr->blob_yuv_min[0] = 0;
    params_ptr->blob_yuv_min[1] = 0;
    params_ptr->blob_yuv_min[2] = 0;
    params_ptr->blob_yuv_max[0] = 0;
    params_ptr->blob_yuv_max[1] = 0;
    params_ptr->blob_yuv_max[2] = 0;
    return 0;
}

int tcp_comms_construct(Tcp_Comms* comms_ptr,
                        MMAL_COMPONENT_T* camera_ptr,
                        Tcp_Params* tcp_params_ptr,
                        unsigned short port_number) {
    comms_ptr->server.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (comms_ptr->server.fd < 0) {
        LOG_ERROR("tcp_comms can't create socket; errno=%d\n", errno);
        return -1;
    }

    comms_ptr->server.saddr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in* saddr_ptr =
                                (struct sockaddr_in*)&comms_ptr->server.saddr;
    saddr_ptr->sin_family = AF_INET;
    saddr_ptr->sin_addr.s_addr = INADDR_ANY;
    saddr_ptr->sin_port = htons(port_number);

    if (bind(comms_ptr->server.fd,
             (struct sockaddr*)saddr_ptr, comms_ptr->server.saddr_len) < 0) {
        LOG_ERROR("tcp_comms can't bind; errno=%d\n", errno);
        return -1;
    }

    if (listen(comms_ptr->server.fd, 3) < 0) {
        LOG_ERROR("tcp_comms can't listen; errno=%d\n", errno);
        return -1;
    }

    // Initialize shared fields of *comms_ptr.

    comms_ptr->camera_ptr = camera_ptr;

    // Start server_loop thread.

    pthread_t pthread_id;
    int status = pthread_create(&pthread_id, NULL, server_thread, comms_ptr);
    if (status != 0) {
        LOG_ERROR("tcp_comms can't pthread_create (%d)\n", status);
        return -1;
    }
    return 0;
}

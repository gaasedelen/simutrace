/*
 * Copyright 2014 (C) Karlsruhe Institute of Technology (KIT)
 * Marc Rittinghaus, Thorsten Groeninger
 *
 *             _____ _                 __
 *            / ___/(_)___ ___  __  __/ /__________ _________
 *            \__ \/ / __ `__ \/ / / / __/ ___/ __ `/ ___/ _ \
 *           ___/ / / / / / / / /_/ / /_/ /  / /_/ / /__/  __/
 *          /____/_/_/ /_/ /_/\__,_/\__/_/   \__,_/\___/\___/
 *                         http://simutrace.org
 *
 * Simutrace Base Library (libsimubase) is part of Simutrace.
 *
 * libsimubase is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libsimubase is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libsimubase. If not, see <http://www.gnu.org/licenses/>.
 */
/*! \file */
#pragma once
#ifndef SIMUBASE_TYPES_H
#define SIMUBASE_TYPES_H

#include "SimuPlatform.h"

#ifdef __cplusplus
namespace SimuTrace {
extern "C"
{
#endif

    /* Object Handle and Id */

#ifdef SIMUTRACE
#if defined(_WIN32)
    typedef HANDLE Handle;
#else
#define INVALID_HANDLE_VALUE -1
    typedef int    Handle;
#endif
#endif

#ifdef __cplusplus
#define INVALID_OBJECT_ID       std::numeric_limits<uint32_t>::max()
#define INVALID_LARGE_OBJECT_ID std::numeric_limits<uint64_t>::max()
#else
#define INVALID_OBJECT_ID       UINT32_MAX
#define INVALID_LARGE_OBJECT_ID UINT64_MAX
#endif
    typedef uint32_t ObjectId;


    /* Globally Unique Identifier (Guid) */

    typedef union _Guid {
        struct {
            uint32_t data1;
            uint16_t data2;
            uint16_t data3;
            uint8_t data4[8];
        };

        struct {
            uint64_t hdata;
            uint64_t ldata;
        };
#if defined(SIMUTRACE) && defined(__cplusplus)
        bool operator <(const _Guid& y) const {
            return std::tie(hdata, ldata) < std::tie(y.hdata, y.ldata);
        }
#endif
    } Guid;

#define DefGuid(a, b, c, d, e, f, g, h, i, j, k) \
    {{a, b, c, {d, e, f, g, h, i, j, k}}}
#define GUID_STRING_LEN (1 + 8 + 1 + 4 + 1 + 4 + 1 + 4 + 1 + 12 + 1)


    /* Exception Handling */

    /*! \brief Type of exception.
     *
     *  The exception class specified to which group of exceptions the error
     *  belongs and allows to interpret the error code accordingly.
     *
     *  \since 3.0
     */
    typedef enum _ExceptionClass {
        EcUnknown           = 0x000,  /*!< The exception class is unknown */

        EcRuntime           = 0x001,  /*!< The exception has been generated by
                                           Simutrace in reaction to a runtime
                                           error such as an invalid argument to
                                           a function call                   */
        EcPlatform          = 0x002,  /*!< The exception has been generated by
                                           the operating system. The error code
                                           can be interpreted with the system
                                           supplied functions                */
        EcPlatformNetwork   = 0x003,  /*!< The exception occurred in the
                                           network stack of the operating
                                           system. The error code can be
                                           interpreted with the system supplied
                                           functions                         */
        EcNetwork           = 0x004,  /*!< The exception has been generated by
                                           Simutrace due to an error in the
                                           communication with the server such
                                           as a malformed RPC message */
        EcUser              = 0x005   /*!< The exception has been generated
                                           due to an error in a user-supplied
                                           callback routine
                                           \since 3.2 */
    } ExceptionClass;


    /*! \brief Location of exception.
     *
     *  Specifies if the exception occurred at the server or the client side.
     *
     *  \since 3.0
     */
    typedef enum _ExceptionSite {
        EsUnknown    = 0x000,  /*!< The exception site is unknown*/

        EsClient     = 0x001,  /*!< The exception occurred on the client side*/
        EsServer     = 0x002   /*!< The exception occurred on the server side*/
    } ExceptionSite;


    /*! \brief Reason for runtime exception.
     *
     *  Specifies the reason for runtime exceptions indicated by an exception
     *  class of EcRuntime.
     *
     *  \since 3.0
     */
    typedef enum _RuntimeException {
        /*! The exception is unknown. */
        RteUnknownException              = 0x000,


        /*! The requested features is not (fully) implemented. Check for a
            newer version of Simutrace */
        RteNotImplementedException       = 0x001,

        /*! The requested object (e.g., a file, a session, a stream, etc.)
            could not be found. The identifier does not point to a valid object
            or the object has been deleted by now */
        RteNotFoundException             = 0x002,

        /*! The requested feature is not supported in the current
            configuration. This might for example be shared memory when using
            sockets. The error might also indicate that a feature is not
            supported by the current version of the client or server. Always
            use the client and server with the same version if possible */
        RteNotSupportedException         = 0x003,


        /*! The requested operation is not valid in the current that of the
            object on which the operation should be performed, for example
            you cannot attach to a session which is already closing */
        RteInvalidOperationException     = 0x010,

        /*! The requested feature or object is not available at the moment
            due to an ongoing operation. Retrying the operation at a later
            time should generally fix this problem. You might encounter this
            exception if you try to read a stream segment that is currently
            still being processed (e.g., compressed) by the server */
        RteOperationInProgressException  = 0x011,

        /*! The requested operation did not finish in the specified amount
            of time */
        RteTimeoutException              = 0x012,


        /*! One or more arguments supplied to a function are not valid. See the
            function's documentation for valid parameter values */
        RteArgumentException             = 0x020,

        /*! One or more pointer arguments passed to a function were \c NULL,
            but are expected to point to valid data or buffer space. See the
            function's documentation for more information */
        RteArgumentNullException         = 0x021,

        /*! The values for one or more arguments passed to a function were out
            of bounds. See the function's documentation for valid values */
        RteArgumentOutOfBoundsException  = 0x022,


        /*! One or more command line options are not valid */
        RteOptionException               = 0x030,

        /*! The supplied configuration is not valid. See the documentation of
            libconfig for more information on the configuration format. See
            the sample configuration for a list of all valid options and their
            default values */
        RteConfigurationException        = 0x031,

        /*! The user-supplied callback raised an exception or returned an
            error

            \since 3.2 */
        RteUserCallbackException         = 0x032
    } RuntimeException;


    /*! \brief Reason for network exception.
     *
     *  Specifies the reason for network exceptions indicated by an exception
     *  class of EcNetwork.
     *
     *  \since 3.0
     */
    typedef enum _NetworkException {
        /*! The network exception is unknown */
        NeUnknownException              = 0x000,

        /*! The RPC message received by the server or client was malformed and
            could not be interpreted. This can happen if the client and server
            are not compatible. Always use the same client and server version
            if possible */
        NeRpcMessageMalformedException  = 0x001
    } NetworkException;


    /* Communication */

#ifdef SIMUTRACE
    /*! \internal \brief Communication channel capabilities. */
    typedef enum _ChannelCapabilities {
        CCapNone           = 0x0000,
        CCapHandleTransfer = 0x0001
    } ChannelCapabilities;
#endif

    /* Clock */

    typedef uint64_t    Timestamp;


    /* File */

#ifdef SIMUTRACE
    typedef uint64_t    FileOffset;
#endif

    /* Others */

 #ifdef SIMUTRACE
    typedef struct _Range {
        uint64_t start;
        uint64_t end;
    } Range;
#endif

#ifdef __cplusplus
#define AddressSet std::unordered_set<uint64_t> // TODO/doom: move this ?
#endif

#ifdef __cplusplus
}
}
#endif

#endif
/*
 * Copyright 2014 (C) Karlsruhe Institute of Technology (KIT)
 * Marc Rittinghaus, Thorsten Groeninger
 *
 * Simutrace Client Library (libsimutrace) is part of Simutrace.
 *
 * libsimutrace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libsimutrace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libsimutrace. If not, see <http://www.gnu.org/licenses/>.
 */
#include "SimuStor.h"

#include "ClientStream.h"

#include "ClientObject.h"

namespace SimuTrace
{

    ClientStream::ClientStream(StreamId id, const StreamDescriptor& desc,
                               StreamBuffer& buffer,
                               ClientSession& session) :
        Stream(id, desc, buffer),
        ClientObject(session),
        _lock(),
        _writeHandle(),
        _readHandles()
    {

    }

    ClientStream::~ClientStream()
    {
        // We do not explicitly close read handles on exit, but instead let
        // the server automatically clean up for us. However, the write handle
        // should be closed by now. If we are using a socket connection, we
        // would lose data otherwise.
        assert(_writeHandle == nullptr);
    }

    void ClientStream::_addHandle(
        std::unique_ptr<StreamStateDescriptor>& handle)
    {
        assert(handle != nullptr);
        assert(handle->stream == this);

        if (!IsSet(handle->flags, StreamStateFlags::SsfRead)) {
            assert(_writeHandle == nullptr);

            _writeHandle = std::move(handle);
        } else {
            _readHandles.push_back(std::move(handle));
        }
    }

    void ClientStream::_releaseHandle(StreamHandle handle)
    {
        assert(handle != nullptr);
        assert(handle->stream == this);
        StreamId id = reinterpret_cast<ClientStream*>(handle->stream)->getId();

        if (handle == _writeHandle.get()) {
            assert(!IsSet(handle->flags, StreamStateFlags::SsfRead));
            assert(!IsSet(handle->flags, StreamStateFlags::SsfDynamic));

            LogDebug("Closing write handle for stream %d.", id);

            _writeHandle = nullptr;
        } else {
            assert(IsSet(handle->flags, StreamStateFlags::SsfRead));

            bool found = false;
            for (auto it = _readHandles.begin();
                 it != _readHandles.end(); ++it) {

                if ((*it).get() == handle) {
                    LogDebug("Closing read handle for stream %d. "
                             "%d handles left.", id, _readHandles.size() - 1);

                    _readHandles.erase(it);
                    found = true;
                    break;
                }
            }

            if (!found) {
                LogWarn("Could not release handle for stream %d. The handle "
                        "could not be found.", id);
            }
        }
    }

    uint64_t ClientStream::queryAddress(StreamSegmentId sequenceNumber,
                                        uint64_t address,
                                        QueryAddressType addressType,
                                        QueryIndexType indexType,
                                        size_t bufferSize, void * bufferOut)
                                        const
    {
        Message response = { 0 };
        StreamId id = this->getId();

        // Prepare outgoing message data
        AddressQuery data = { address, sequenceNumber, addressType, indexType, _false};

        // If the user provides a buffer, the server must return instance data
        auto hasBuffers = (bufferSize != 0 && bufferOut != nullptr) ? _true : _false;
        data.returnData = hasBuffers;

        // Send the client address query to the server
        _getPort().call(&response, RpcApi::CCV_QueryAddress, &data,
                        sizeof(AddressQuery), id);

        ThrowOn((response.payloadType != MessagePayloadType::MptData),
            RpcMessageMalformedException);

        if (hasBuffers)
        {
            assert(response.data.payload != nullptr);
            const size_t size = response.data.payloadLength;
            const size_t copySize = (size < bufferSize) ? size : bufferSize;
            memcpy(bufferOut, response.data.payload, copySize);
        }

        // return the 'total' number of hits for the given query (spread across param 1 & 2)
        return (((uint64_t)response.parameter0) << 32) | response.data.parameter1;
    }

    StreamHandle ClientStream::append(StreamHandle handle)
    {
        LockScope(_lock);

        if (handle != nullptr) {
            ThrowOn(handle->stream != this, InvalidOperationException);
            ThrowOn(IsSet(handle->flags, StreamStateFlags::SsfRead),
                    InvalidOperationException);
        }

        return _append(handle);
    }

    StreamHandle ClientStream::open(QueryIndexType type, uint64_t value,
                                    StreamAccessFlags flags, StreamHandle handle)
    {
        LockScope(_lock);

        // We do not check here, if the supplied handle is in our list or if
        // it is a manually crafted one by the caller. However, we do not need
        // to care.
        if (handle != nullptr) {
            ThrowOn(handle->stream != this, InvalidOperationException);
            ThrowOn(!IsSet(handle->flags, StreamStateFlags::SsfRead),
                    InvalidOperationException);
        }

        return _open(type, value, flags, handle);
    }

    void ClientStream::close(StreamHandle handle)
    {
        LockScope(_lock);

        ThrowOnNull(handle, ArgumentNullException, "handle");
        ThrowOn(handle->stream != this, InvalidOperationException);

        if (IsSet(handle->flags, StreamStateFlags::SsfDynamic) ||
            (handle->stat.control != nullptr)) {
            _closeHandle(handle);
        }

        _releaseHandle(handle);
    }

    void ClientStream::flush()
    {
        LockScope(_lock);

        if (_writeHandle == nullptr) {
            return;
        }

        // For a stream to contain new data, a corresponding stream handle has
        // to be allocated. A write handle therefore points to a segment that
        // needs to be submitted. However, for segments backed by a
        // shared-memory buffer, we let the server submit it automatically.

        if (getStreamBuffer().isMaster()) {
            _closeHandle(_writeHandle.get());
        }

        _releaseHandle(_writeHandle.get());
    }

}
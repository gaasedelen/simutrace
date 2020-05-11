/*
 * Copyright 2014 (C) Karlsruhe Institute of Technology (KIT)
 * Marc Rittinghaus, Thorsten Groeninger
 *
 * Simutrace Store Library (libsimustor) is part of Simutrace.
 *
 * libsimustor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libsimustor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libsimustor. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef STORE_H
#define STORE_H

#include "SimuBase.h"
#include "SimuStorTypes.h"

namespace SimuTrace
{

    class StreamBuffer;
    class Stream;

    struct StorageLocation
    {
        const StreamSegmentLink link;

        StreamRangeInformation ranges;

        uint64_t compressedSize;
        uint32_t rawEntryCount;

        StorageLocation(StreamSegmentLink link) :
            link(link),
            ranges(),
            compressedSize(0),
            rawEntryCount(0) { }

        StorageLocation(SegmentControlElement& ctrl) :
            link(ctrl.link),
            ranges(),
            compressedSize(0),
            rawEntryCount(ctrl.rawEntryCount)
        {
            assert(ctrl.rawEntryCount > 0);
            if (ctrl.startIndex != INVALID_ENTRY_INDEX) {
                ranges.startIndex = ctrl.startIndex;
                ranges.endIndex = ctrl.startIndex + ctrl.rawEntryCount - 1;
            }
            ranges.startCycle = ctrl.startCycle;
            ranges.endCycle = ctrl.endCycle;
            ranges.startTime = ctrl.startTime;
            ranges.endTime = ctrl.endTime;
        }

        virtual ~StorageLocation() { }

        uint32_t getEntryCount() const
        {
            if (ranges.startIndex == INVALID_ENTRY_INDEX) {
                assert(ranges.endIndex == INVALID_ENTRY_INDEX);
                return 0;
            }

            assert(ranges.startIndex <= ranges.endIndex);

            return static_cast<uint32_t>(ranges.endIndex - ranges.startIndex) + 1;
        }
    };

    class Store
    {
    public:
        typedef ObjectReference<Store> Reference;

        static Reference makeOwnerReference(Store* store);
        static Reference makeUserReference(Reference& ownerReference);
    private:
        DISABLE_COPY(Store);

        bool _configurationLocked;

        StoreId _id;
        std::string _name;

        uint32_t _numRegularStreams;
        uint32_t _numDynamicStreams;

        std::map<BufferId, std::unique_ptr<StreamBuffer>> _buffers;
        std::map<StreamId, std::unique_ptr<Stream>> _streams;
    protected:
        mutable ReaderWriterLock _lock;

        Store(StoreId id, const std::string& name);

        virtual std::unique_ptr<StreamBuffer> _createStreamBuffer(
            size_t segmentSize, uint32_t numSegments) = 0;
        virtual std::unique_ptr<Stream> _createStream(StreamId id,
            StreamDescriptor& desc, BufferId buffer) = 0;

        virtual void _enumerateStreamBuffers(std::vector<BufferId>& out) const = 0;
        virtual void _enumerateStreams(std::vector<StreamId>& out,
                                       StreamEnumFilter filter) const = 0;

        virtual bool _supportsWriteAfterOpen() const { return false; }

        void _lockConfiguration();
        void _freeConfiguration();

        BufferId _addStreamBuffer(std::unique_ptr<StreamBuffer>& buffer);
        StreamId _addStream(std::unique_ptr<Stream>& stream);

        BufferId _registerStreamBuffer(size_t segmentSize, uint32_t numSegments);
        StreamId _registerStream(StreamId id, StreamDescriptor& desc,
                                 BufferId buffer);

        void _enumerateStreamBuffers(std::vector<StreamBuffer*>& out) const;
        void _enumerateStreams(std::vector<Stream*>& out,
                               StreamEnumFilter filter) const;

        virtual StreamBuffer* _getStreamBuffer(BufferId id);
        virtual Stream* _getStream(StreamId id);
    public:
        virtual ~Store();

        BufferId registerStreamBuffer(size_t segmentSize, uint32_t numSegments);
        StreamId registerStream(StreamDescriptor& desc, BufferId buffer);

        void enumerateStreamBuffers(std::vector<BufferId>& out) const;
        void enumerateStreams(std::vector<StreamId>& out,
                              StreamEnumFilter filter) const;

        uint32_t summarizeStreamStats(StreamStatistics& stats,
                                      uint64_t& uncompressedSize,
                                      StreamEnumFilter filter) const;

        StoreId getId() const;
        const std::string& getName() const;

        StreamBuffer& getStreamBuffer(BufferId id);

        Stream& getStream(StreamId id);
        Stream* findStream(StreamId id);
    };

}

#endif
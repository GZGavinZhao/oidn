// Copyright 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/common.h"
#include "ref.h"
#include <unordered_map>

OIDN_NAMESPACE_BEGIN

  struct TensorDesc;
  struct ImageDesc;

  class Memory;
  class Tensor;
  class Image;

  class Device;
  class Engine;

  // Access modes for mapping buffers
  enum class Access
  {
    Read,         // read-only access
    Write,        // write-only access
    ReadWrite,    // read and write access
    WriteDiscard, // write-only access, previous contents discarded
  };

  // -----------------------------------------------------------------------------------------------
  // Buffer
  // -----------------------------------------------------------------------------------------------

  // Generic buffer object
  class Buffer : public RefCount
  {
    friend class Memory;

  public:
    virtual Engine* getEngine() const = 0;
    Device* getDevice() const;

    virtual bool hasPtr() const = 0;  // not all buffers have a pointer to their data
    virtual char* getPtr() const = 0; // nullptr if !hasPtr()
    virtual size_t getByteSize() const = 0;
    virtual Storage getStorage() const = 0;

    virtual void read(size_t byteOffset, size_t byteSize, void* dstHostPtr, SyncMode sync = SyncMode::Sync);
    virtual void write(size_t byteOffset, size_t byteSize, const void* srcHostPtr, SyncMode sync = SyncMode::Sync);

    virtual void* map(size_t byteOffset, size_t byteSize, Access access);
    virtual void unmap(void* hostPtr);

    // Reallocates the buffer with a new size discarding its current contents
    virtual void realloc(size_t newByteSize);

    std::shared_ptr<Tensor> newTensor(const TensorDesc& desc, size_t byteOffset);
    std::shared_ptr<Image> newImage(const ImageDesc& desc, size_t byteOffset);

  private:
    // Memory objects backed by the buffer must attach themselves
    virtual void attach(Memory* mem) {}
    virtual void detach(Memory* mem) {}
  };

  // -----------------------------------------------------------------------------------------------
  // MappedBuffer
  // -----------------------------------------------------------------------------------------------

  // Memory mapped version of a buffer
  class MappedBuffer final : public Buffer
  {
  public:
    MappedBuffer(const Ref<Buffer>& buffer, size_t byteOffset, size_t byteSize, Access access);
    ~MappedBuffer();

    Engine* getEngine() const override { return buffer->getEngine(); }

    bool hasPtr() const override { return true; }
    char* getPtr() const override { return ptr; }
    size_t getByteSize() const override { return byteSize; }
    Storage getStorage() const override { return Storage::Host; }

  private:
    char* ptr;
    size_t byteSize;
    Ref<Buffer> buffer;
  };

  // -----------------------------------------------------------------------------------------------
  // USMBuffer
  // -----------------------------------------------------------------------------------------------

  // Unified shared memory (USM) based buffer object
  class USMBuffer : public Buffer
  {
  public:
    USMBuffer(const Ref<Engine>& engine, size_t byteSize, Storage storage);
    USMBuffer(const Ref<Engine>& engine, void* data, size_t byteSize, Storage storage = Storage::Undefined);
    ~USMBuffer();

    Engine* getEngine() const override { return engine.get(); }

    bool hasPtr() const override { return true; }
    char* getPtr() const override { return ptr; }
    size_t getByteSize() const override { return byteSize; }
    Storage getStorage() const override { return storage; }

    void* map(size_t byteOffset, size_t byteSize, Access access) override;
    void unmap(void* hostPtr) override;

    void read(size_t byteOffset, size_t byteSize, void* dstHostPtr, SyncMode sync) override;
    void write(size_t byteOffset, size_t byteSize, const void* srcHostPtr, SyncMode sync) override;

    void realloc(size_t newByteSize) override;

  protected:
    explicit USMBuffer(const Ref<Engine>& engine);
    void unmapAll();

    char* ptr;
    size_t byteSize;
    bool shared;
    Storage storage;

  private:
    struct MappedRegion
    {
      void* devPtr;
      size_t byteOffset;
      size_t byteSize;
      Access access;
    };

    std::unordered_map<void*, MappedRegion> mappedRegions;
    Ref<Engine> engine;
  };

  // -----------------------------------------------------------------------------------------------
  // Memory
  // -----------------------------------------------------------------------------------------------

  // Memory object optionally backed by a buffer
  class Memory
  {
  public:
    Memory() : byteOffset(0) {}

    Memory(const Ref<Buffer>& buffer, size_t byteOffset = 0)
      : buffer(buffer),
        byteOffset(byteOffset)
    {
      buffer->attach(this);
    }

    virtual ~Memory()
    {
      if (buffer)
        buffer->detach(this);
    }

    Buffer* getBuffer() const { return buffer.get(); }
    size_t getByteOffset() const { return byteOffset; }

    // If the buffer gets reallocated, this must be called to update the internal pointer
    virtual void updatePtr() = 0;

  protected:
    Ref<Buffer> buffer; // buffer containing the data
    size_t byteOffset;  // offset in the buffer
  };

OIDN_NAMESPACE_END

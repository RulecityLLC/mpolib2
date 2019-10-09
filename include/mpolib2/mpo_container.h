#ifndef MPO_CONTAINER_H
#define MPO_CONTAINER_H

#include "mpo_deleter.h"
#include "mpo_stream.h"

typedef bool (*CallbackProc)(MPO_UINT64 u64Finished, MPO_UINT64 u64Total);

class IMpoContainer {
public:

    // navigation

    // seek to an arbitrary blob
    virtual bool JumpToBlob(MPO_UINT64 uBlobIdx) = 0;

    // reading
    virtual bool ReadHeader() = 0;

    virtual bool StartReadBlob(unsigned int &uID) = 0;

    // must only be called after StartReadBlob and before EndReadBlob
    virtual MPO_UINT64 GetCurBlobSizebytes() = 0;

    virtual size_t ReadFromBlob(void *buf, size_t stNumBytes) = 0;

    // will return false if blob was corrupt (MD5 check failed)
    virtual bool EndReadBlob() = 0;

    // writing
    virtual bool WriteHeader() = 0;

    // start writing to the next blob (must be at the end of a previous blob or the header)
    virtual bool StartWriteBlob(unsigned int id) = 0;

    virtual size_t WriteToBlob(const void *buf, size_t stNumBytes) = 0;

    // must be called to finalize a blob before moving on to the next one
    virtual bool EndWriteBlob() = 0;

    // Closes the container and returns true if any last minute writes succeeded
    virtual bool Close(CallbackProc pCallback = 0, size_t stUpdateIntervalBytes = (1 << 22)) = 0;

    // info
    virtual MPO_UINT64 GetBlobIdx() const = 0;

    virtual MPO_UINT64 GetBlobCount() const = 0;

    virtual MPO_UINT64 GetMaxBlobSizeBytes() const = 0;

    // Verifies integrity of entire container. Can only be done when reading from the container, right after reading the header.
    // The callback gives progress updates and if the callback returns false, the verify process will stop.
    // 'stUpdateIntervalBytes' is how often the callback should be called.
    // Returns true if container's integrity check passed, or false on failure.
    virtual bool VerifyContainer(CallbackProc pCallback = 0, size_t stUpdateIntervalBytes = (1 << 22)) = 0;
};

typedef shared_ptr<IMpoContainer> IMpoContainerSPtr;

class EXPORT_ME MpoContainerFactory
{
public:
	static IMpoContainerSPtr CreateInstance(IBlockingStream *pStream, bool bIgnoreReadIntegrity = false);
};

// THIS IS A WRAPPER CLASS
// It's purpose is to make it cleaner to write jpegs to containers for use with Daphne.
// It is write-only (for now).
class EXPORT_ME MpoContainerStream : public IBlockingStream, public MpoDeleter
{
public:
	static blocking_sharedptr CreateInstance (IMpoContainer *pCon);

	size_t Read(void *buf, size_t stBytesToRead);

	size_t Write(const void *buf, size_t stBytesToWrite);

	bool Seek(MPO_INT64 i64Offset, seek_type origin);

	MPO_UINT64 GetLength();

	MPO_UINT64 GetPosition();

	bool CanRead();

	bool CanWrite();

	bool CanSeek();

private:
	MpoContainerStream() { }

	virtual ~MpoContainerStream() { }

	void DeleteInstance() { delete this; }

	IMpoContainer *m_pCon;
};

#endif // MPO_CONTAINER_H

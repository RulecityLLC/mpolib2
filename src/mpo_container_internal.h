//
// Created by Matt on 10/4/2019.
//

#ifndef MPO2_MPO_CONTAINER_INTERNAL_H
#define MPO2_MPO_CONTAINER_INTERNAL_H

#include <mpolib/mpo_container.h>
#include <mpolib/mpo_deleter.h>
#include <mpolib/mpo_stream.h>
#include "md5_global.h"
#include "md5.h"

// to keep track of blob Table Of Contents
#include <vector>
using namespace std;

struct ContainerHeader
{
    unsigned int uVersion;
    MPO_UINT64 u64TotalBlobs;
    MPO_UINT64 u64OffsetTOC;
    unsigned char TotalMD5[16];
};

class MpoContainer : public IMpoContainer, public MpoDeleter
{
    friend class MpoContainerFactory;

public:

// navigation

// seek to an arbitrary blob
bool JumpToBlob(MPO_UINT64 uBlobIdx);

// reading
bool ReadHeader();

bool StartReadBlob(unsigned int &uID);

// must only be called after StartReadBlob and before EndReadBlob
MPO_UINT64 GetCurBlobSizebytes();

size_t ReadFromBlob(void *buf, size_t stNumBytes);

// will return false if blob was corrupt (MD5 check failed)
bool EndReadBlob();

// writing
bool WriteHeader();

// start writing to the next blob (must be at the end of a previous blob or the header)
bool StartWriteBlob(unsigned int id);

size_t WriteToBlob(const void *buf, size_t stNumBytes);

// must be called to finalize a blob before moving on to the next one
bool EndWriteBlob();

// Closes the container and returns true if any last minute writes succeeded
bool Close(CallbackProc pCallback = 0, size_t stUpdateIntervalBytes = (1<<22));

// info
MPO_UINT64 GetBlobIdx() const;

MPO_UINT64 GetBlobCount() const;

MPO_UINT64 GetMaxBlobSizeBytes() const;

// Verifies integrity of entire container. Can only be done when reading from the container, right after reading the header.
// The callback gives progress updates and if the callback returns false, the verify process will stop.
// 'stUpdateIntervalBytes' is how often the callback should be called.
// Returns true if container's integrity check passed, or false on failure.
bool VerifyContainer(CallbackProc pCallback = 0, size_t stUpdateIntervalBytes = (1<<22));

private:
	void DeleteInstance() { delete this; }

	MpoContainer();
	~MpoContainer();

////

IBlockingStream *m_pStream;

// Policy: as of now we only allow reading or writing to the container, not a mixture of both
// whether header has been read
bool m_bReadHeader;
// whether header has been written
bool m_bWroteHeader;

// whether we need to update the header when we close the container
bool m_bHeaderNeedsUpdate;

// whether StartWriteBlob was just called (and hence writing is allowed)
bool m_bWriteBlobStarted;

// how many bytes of the current blob have been written (so we know the total size)
MPO_UINT64 m_u64CurBytesWritten;

// offset where current blob starts
MPO_UINT64 m_u64CurBlobStartOffset;

// used only when reading
MPO_UINT64 m_u64BlobSizeBytes;

// Blob's MD5 (used only when reading)
unsigned char m_arrBlobMD5[16];

// how many bytes of the current blob have been read
MPO_UINT64 m_u64CurBytesRead;

// whether StartReadBlob has been called, meaning reading is allowed
bool m_bReadBlobStarted;

// stores header that we've read (or that we will write)
ContainerHeader m_Header;

// md5 for the current blob we're reading/writing
oMD5_CTX m_md5CurBlob;

// md5 for the entire container that we're reading/writing (probably will just use for writing)
oMD5_CTX m_md5Container;

// contains the starting offset of each blob for our Table Of Contents
vector<MPO_UINT64> m_vTOC;

// the blob we are currently reading
MPO_UINT64 m_u64CurBlobIdx;

// max size of any blob in this container (computed by ReadHeader only)
MPO_UINT64 m_u64MaxBlobSizeBytes;

// if true, MD5 check will be skipped when reading
bool m_bIgnoreReadIntegrity;

};

#endif //MPO2_MPO_CONTAINER_INTERNAL_H

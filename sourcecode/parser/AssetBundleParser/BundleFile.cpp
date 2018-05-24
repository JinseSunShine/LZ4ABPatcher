#include "lz4.h"
#include <assert.h>
#include <string.h>
#include "Parser.h"
#include "EntryItem.h"
#include "BundleFile.h"
#include "AssetChunk.h"
#include "EndianBinaryReader.h"

extern "C" void md5(const char *message, long len, char *output);

BundleFile::BundleFile(EndianBinaryReader* reader)
{
	m_headerReader = reader;
}

BundleFile::~BundleFile()
{
	for (int i = 0; i < m_asset_packge.size(); i++)
	{
		SAFE_DELETE(m_asset_packge[i]);
	}
	m_asset_packge.clear();

	//�����ڴ�
	for (int i = 0; i < m_blocklist.size(); i++)
	{
		SAFE_DELETE(m_blocklist[i]);
	}
	m_blocklist.clear();

	SAFE_DELETE(m_chunkReader);
	SAFE_DELETE(m_headerReader);

	if (m_blockReader != NULL)
	{
		void* blockBuffer = (void*)m_blockReader->RetPointer(0);
		SAFE_FREE(blockBuffer);
		SAFE_DELETE(m_blockReader);
	}
}

bool BundleFile::Parse(BundleFileParser* parser)
{
	assert(parser != NULL && m_headerReader != NULL);
	m_headerReader->SetPosition(0);
	m_signature = m_headerReader->ReadStringToNull();
	if (strcmp(m_signature.c_str(), "UnityFS"))
	{
		push_message("unmatch signature %s", m_signature.c_str());
		return false;
	}

	m_format = m_headerReader->ReadInt32();
	if (m_format != 6)
	{
		assert(false);
		push_message("unmatch version %d", m_format);
		return false;
	}	

	m_versionPlayer = m_headerReader->ReadStringToNull();
	m_versionEngine = m_headerReader->ReadStringToNull();
	return ReadFormat6(parser, m_headerReader);
}

void BundleFile::PartitionChunks(std::vector<AssetChunk*>& src, std::vector<AssetChunk*>& dst, int offset, int length, int& bytes_trimed, int& bytes_compress, int& bytes_uncompress)
{
	dst.clear();
	bytes_trimed = bytes_compress = bytes_uncompress = 0;
	if (src.size() <= 0)
	{
		return;
	}

	int chunkset_begin = offset;
	int chunkset_end = chunkset_begin + length;

	bytes_trimed = -1;
	int chunk_begin = 0, chunk_end = 0;
	for (int index = 0; index < src.size(); index++)
	{
		AssetChunk* block = src[index];
		chunk_end += block->GetUnCompressedSize();
		if (chunk_end >= chunkset_begin)
		{
			if (chunk_begin + 1 > chunkset_end)
			{
				break;
			}
			else
			{
				if (bytes_trimed < 0)
				{
					bytes_trimed = chunk_begin;
				}
				bytes_compress += block->GetCompressedSize();
				bytes_uncompress += block->GetUnCompressedSize();
				dst.push_back(block);
			}
		}
		chunk_begin = chunk_end;
	}
}

bool BundleFile::ReadFormat6(BundleFileParser* parser, EndianBinaryReader* file_reader)
{
	bool success = true;
	//[���ļ�ͷ
	m_bundleSizePos = file_reader->GetPosition();
	m_bundleSize = file_reader->ReadInt64();
	m_blockstartpos = file_reader->GetPosition();
	m_tableCompressedSize = file_reader->ReadInt32();
	m_tableUncompressedSize = file_reader->ReadInt32();
	m_flag = file_reader->ReadInt32();

	int position = file_reader->GetPosition();
	unsigned char* buff = (unsigned char*)malloc(m_tableCompressedSize + m_tableUncompressedSize);
	AssetChunk* header = new AssetChunk(file_reader, m_tableCompressedSize, m_tableUncompressedSize, m_flag);
	m_tableUncompressedSize = header->GetUnCompressData(buff + m_tableUncompressedSize, m_tableCompressedSize, buff, m_tableUncompressedSize);
	int bundleTableEnd = file_reader->GetPosition();
	file_reader->SetPosition(0);
	parser->EndParseBundleTable(this, file_reader, bundleTableEnd);
	file_reader->SetPosition(bundleTableEnd);
	if ((m_flag & 0x80) != 0)//at end of file
	{
		assert(false);
		push_message("nonsupport flag %x", m_flag);
		file_reader->SetPosition(position);
	}
	//]���ļ�ͷ

	//[��ȡbundle����Ϣ
	m_blockReader = EndianBinaryReader::Create(buff, m_tableUncompressedSize);
	m_blockReader->Prepare(0x10);

	long long chunkAllSize = 0;
	int blockcount = m_blockReader->ReadInt32();
	parser->BeginParseChunksInfo(blockcount);
	if (blockcount > 0)
	{
		m_blocklist.resize(blockcount);
		for (int index = 0; index < blockcount; index++)
		{
			int uncompressedSize = m_blockReader->ReadInt32();
			int compressedSize = m_blockReader->ReadInt32();
			short flag = m_blockReader->ReadInt16();
			int nextpos = file_reader->GetPosition() + compressedSize;
			m_blocklist[index] = new AssetChunk(file_reader, compressedSize, uncompressedSize, flag);
			parser->ParseChunkInfo(index, uncompressedSize, compressedSize, flag);
			file_reader->SetPosition(nextpos);
			chunkAllSize += uncompressedSize;
		}
	}
	parser->EndParseChunksInfo();

	int entryinfo_count = m_blockReader->ReadInt32();
	parser->CreateEntryinfo(entryinfo_count);
	m_chunkReader = EndianBinaryReader::Create(m_blocklist, 0, chunkAllSize);
	for (int i = 0; i < entryinfo_count; i++)
	{
		long long entryinfo_offset = m_blockReader->ReadInt64();
		long long entryinfo_size = m_blockReader->ReadInt64();
		int flag = m_blockReader->ReadInt32();
		std::string packgename = m_blockReader->ReadStringToNull();
		if (parser->FillEntryinfo(i, packgename, m_chunkReader, entryinfo_offset, entryinfo_size, flag) == false)
		{
			success = false;
		}
	}
	//]��ȡbundle����Ϣ
	parser->EndParserEntrys();
	parser->EndParseBundleInfo(this);
	return success;
}

long long assetbundle_size(const char* path)
{
	if (path == NULL || *path == 0)
	{
		return 0;
	}

	FILE* file = fopen(path, "rb");
	if (file == NULL)
	{
		return 0;
	}

	long long bundleSize = 0;
	EndianBinaryReader* reader = EndianBinaryReader::Create(file);
	std::string signature = reader->ReadStringToNull();
	if (memcmp(signature.c_str(), "UnityFS", 8) == 0)
	{
		if (reader->ReadInt32() == 6)
		{
			std::string versionPlayer = reader->ReadStringToNull();
			std::string versionEngine = reader->ReadStringToNull();
			bundleSize = reader->ReadInt64();
			if (reader->GetLength() < bundleSize)
			{
				bundleSize = 0;
			}
		}
	}
	SAFE_DELETE(reader);
	fclose(file);
	return bundleSize;
}

bool assetbundle_check(const char* path)
{
	return assetbundle_size(path) > 0;
}
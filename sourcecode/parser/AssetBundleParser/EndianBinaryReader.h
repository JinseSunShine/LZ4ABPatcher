#ifndef _ENDIAN_BINARY_READER_H_
#define _ENDIAN_BINARY_READER_H_

#include <list>
#include <vector>
#include <string>
#include <stdio.h>
#include <malloc.h>

class AssetChunk;

#define CHUNK_LENGTH (128 * 1024)
#define SAFE_FREE(POINTER) if(POINTER != NULL){ free(POINTER); POINTER = NULL;}
#define SAFE_DELETE(POINTER) if(POINTER != NULL){ delete(POINTER); POINTER = NULL;}
#define SAFE_CLOSE(POINTER) if(POINTER != NULL){ fclose(POINTER); POINTER = NULL;}

enum EndianType
{
	BigEndian,
	LittleEndian
};

class EndianBinaryReader
{
public:
	static EndianBinaryReader* Create(FILE* file);
	static EndianBinaryReader* Create(const void* buf, int buf_len);
	static EndianBinaryReader* Create(const std::vector<AssetChunk*>& chunks, int discard, int chunk_length);

	static bool Reverse(void* var, int length);
	virtual bool ReadBoolean();
	virtual unsigned char ReadByte();
	virtual void AlignStream(int alignment);
	virtual short ReadInt16();
	virtual int ReadInt32();
	virtual unsigned int ReadUInt32();
	virtual long long ReadInt64();
	virtual char* ReadStringToNull() = 0;
	virtual void ReadStringToNull(char* buf, int length) = 0;
	virtual void* ReadBytes(int length) = 0;
	virtual int ReadBytes(void* buf, int length) = 0;
	virtual int GetLength() = 0;
	virtual int GetPosition() = 0;
	virtual void SetPosition(int position) = 0;
	virtual EndianBinaryReader* Clone() = 0;
	virtual void Trim();
	virtual void LockTrim(bool lock);
	virtual const void* RetPointer(int pos);
	virtual int Prepare(int length);

public:
	EndianType GetEndianType();
	void SetEndianType(EndianType type);
	EndianBinaryReader();
	virtual ~EndianBinaryReader();
protected:
	EndianType m_endian;
};

class EndianBinaryReaderForFile : public EndianBinaryReader
{
public:
	virtual char* ReadStringToNull();
	virtual void ReadStringToNull(char* buf, int length);
	virtual void* ReadBytes(int length);
	virtual int ReadBytes(void* buf, int length);
	virtual int GetLength();
	virtual int GetPosition();
	virtual void SetPosition(int position);
	virtual EndianBinaryReader* Clone();
protected:
	friend class EndianBinaryReader;
	EndianBinaryReaderForFile(FILE* file);
	virtual ~EndianBinaryReaderForFile();
protected:
	FILE* m_file;
	int m_file_length;
};

class EndianBinaryReaderForMemory : public EndianBinaryReader
{
public:
	virtual const void* RetPointer(int offset);
	virtual char* ReadStringToNull();
	virtual void ReadStringToNull(char* buf, int length);
	virtual void* ReadBytes(int length);
	virtual int ReadBytes(void* buf, int length);
	virtual int GetLength();
	virtual int GetPosition();
	virtual void SetPosition(int position);
	virtual EndianBinaryReader* Clone();

protected:
	friend class EndianBinaryReader;
	EndianBinaryReaderForMemory(const char* buf, int buf_len);
	virtual ~EndianBinaryReaderForMemory();
protected:
	int m_position;
	const char* m_buf;
	int m_buf_len;
};

class EndianBinaryReaderForChunk : public EndianBinaryReader
{
public:
	virtual const void* RetPointer(int offset);
	virtual char* ReadStringToNull();
	virtual void ReadStringToNull(char* buf, int length);
	virtual void* ReadBytes(int length);
	virtual int ReadBytes(void* buf, int length);
	virtual int GetLength();
	virtual int GetPosition();
	virtual void SetPosition(int position);
	virtual EndianBinaryReader* Clone();
	virtual void Trim();
	virtual void LockTrim(bool lock);
	virtual int Prepare(int length);

protected:
	friend class EndianBinaryReader;
	int PositionToChunkIndex(int position);
	int PositionToBufferOffset(int position);
	void Ensure(int index, int length);
	EndianBinaryReaderForChunk(EndianBinaryReaderForChunk* reader);
	EndianBinaryReaderForChunk(std::vector<AssetChunk*> chunks, int discard, int chunk_length);
	virtual ~EndianBinaryReaderForChunk();
protected:
	int m_trim_lock;

	//[chunk����
	struct AssetChunkData
	{
		AssetChunk* chunk;
		int virtualpos;
		int memorypos;
	};
	int m_bytes_discard;						//��һ����֮ǰ�������ڴ棬��ȡʱ��Ҫ�ų�����
	int m_chunks_bytes_length;					//����chunk��ѹ���С�ܺͣ�
	std::vector<AssetChunkData*> m_chunks;		//ԭʼ���ݿ���ڻ������Ӧ������
	//]chunk����

	//[�ڴ��
	/* 0:��ȡԭʼ���ݣ���ʼ��СCHUNK_LENGTH
	// 1:�洢��ѹ������ݣ���С����ʵ���������ʱ����
	*/
	int m_position;
	int m_buffer_used;							//�Ѿ�ʹ�õĻ����С;
	int m_chunk_readed_start;					//�Ѿ���ȡ��chunk��ʼ��;
	int m_chunk_readed_count;					//�Ѿ���ȡ��chunk������;
	void* m_buffers[2];
	int m_buffer_length[2];
	//]�ڴ��
};

#endif // !_ENDIAN_BINARY_READER_H_
#include "pch.h"
#include "data.h"
#include "saved_data.h"
#include "stream.h"

namespace
{
    class reader_stream : public IStream
    {
    public:
        reader_stream(const std::shared_ptr<ff::reader_base>& reader)
            : reader(reader)
            , refs(0)
        {}

        virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (!ppvObject)
            {
                return E_POINTER;
            }
            else if (riid == __uuidof(IUnknown))
            {
                *ppvObject = static_cast<IUnknown*>(this);
            }
            else if (riid == __uuidof(IStream))
            {
                *ppvObject = static_cast<IStream*>(this);
            }
            else
            {
                return E_NOINTERFACE;
            }

            this->AddRef();
            return S_OK;
        }

        virtual ULONG __stdcall AddRef() override
        {
            return this->refs.fetch_add(1) + 1;
        }

        virtual ULONG __stdcall Release() override
        {
            ULONG refs = this->refs.fetch_sub(1) - 1;
            if (!refs)
            {
                delete this;
            }

            return refs;
        }

        virtual HRESULT __stdcall Read(void* pv, ULONG cb, ULONG* pcbRead) override
        {
            size_t cb2 = static_cast<size_t>(cb);
            size_t bytes_left = this->reader->size() - this->reader->pos();
            size_t bytes_read = std::min(cb2, bytes_left);

            if (bytes_read)
            {
                bytes_read = this->reader->read(pv, bytes_read);
            }

            if (pcbRead)
            {
                *pcbRead = static_cast<ULONG>(bytes_read);
            }

            return (bytes_read == cb2) ? S_OK : S_FALSE;
        }

        virtual HRESULT __stdcall Write(const void* pv, ULONG cb, ULONG* pcbWritten) override
        {
            return STG_E_CANTSAVE;
        }

        virtual HRESULT __stdcall Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) override
        {
            LARGE_INTEGER new_pos;
            switch (dwOrigin)
            {
                case STREAM_SEEK_SET:
                    new_pos = dlibMove;
                    break;

                case STREAM_SEEK_CUR:
                    new_pos.QuadPart = static_cast<LONGLONG>(this->reader->pos()) + dlibMove.QuadPart;
                    break;

                case STREAM_SEEK_END:
                    new_pos.QuadPart = static_cast<LONGLONG>(this->reader->size()) + dlibMove.QuadPart;
                    break;

                default:
                    return STG_E_INVALIDFUNCTION;
            }

            size_t new_pos2 = static_cast<size_t>(new_pos.QuadPart);
            if (this->reader->pos(new_pos2) != new_pos2)
            {
                return STG_E_INVALIDFUNCTION;
            }

            if (plibNewPosition)
            {
                plibNewPosition->QuadPart = static_cast<ULONGLONG>(new_pos.QuadPart);
            }

            return S_OK;
        }

        virtual HRESULT __stdcall SetSize(ULARGE_INTEGER libNewSize) override
        {
            return STG_E_INVALIDFUNCTION;
        }

        virtual HRESULT __stdcall CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) override
        {
            if (!pstm || pstm == this)
            {
                return STG_E_INVALIDPOINTER;
            }

            size_t bytes_requested = static_cast<size_t>(cb.QuadPart);
            size_t bytes_left = this->reader->size() - this->reader->pos();
            size_t bytes_read = std::min(bytes_requested, bytes_left);

            if (pcbRead)
            {
                pcbRead->QuadPart = static_cast<ULONGLONG>(bytes_read);
            }

            if (bytes_read)
            {
                ff::stack_vector<uint8_t, 1024> buffer;
                buffer.resize(bytes_read);
                if (this->reader->read(buffer.data(), bytes_read) != bytes_read)
                {
                    return E_FAIL;
                }

                ULONG bytes_written = 0;
                HRESULT hr = pstm->Write(buffer.data(), static_cast<ULONG>(bytes_read), &bytes_written);

                if (pcbWritten)
                {
                    pcbWritten->QuadPart = static_cast<ULONGLONG>(bytes_written);
                }

                return hr;
            }
            else
            {
                if (pcbWritten)
                {
                    pcbWritten->QuadPart = 0;
                }

                return S_OK;
            }
        }

        virtual HRESULT __stdcall Commit(DWORD grfCommitFlags) override
        {
            return S_OK;
        }

        virtual HRESULT __stdcall Revert() override
        {
            return S_OK;
        }

        virtual HRESULT __stdcall LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override
        {
            return STG_E_INVALIDFUNCTION;
        }

        virtual HRESULT __stdcall UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override
        {
            return STG_E_INVALIDFUNCTION;
        }

        virtual HRESULT __stdcall Stat(STATSTG* pstatstg, DWORD grfStatFlag) override
        {
            if (!pstatstg)
            {
                return STG_E_INVALIDPOINTER;
            }

            std::memset(pstatstg, 0, sizeof(*pstatstg));

            pstatstg->type = STGTY_STREAM;
            pstatstg->cbSize.QuadPart = static_cast<ULONGLONG>(this->reader->size());

            return S_OK;
        }

        virtual HRESULT __stdcall Clone(IStream** ppstm) override
        {
            return STG_E_INVALIDFUNCTION;
        }

    protected:
        std::atomic_ulong refs;
        std::shared_ptr<ff::reader_base> reader;
    };
}

ff::data_reader::data_reader(const std::shared_ptr<data_base>& data)
    : data(data)
    , data_pos(0)
{}

size_t ff::data_reader::read(void* data, size_t size)
{
    size = std::min(size, this->data->size() - this->data_pos);
    std::memcpy(data, this->data->data() + this->data_pos, size);
    this->data_pos += size;
    return size;
}

size_t ff::data_reader::size() const
{
    return this->data->size();
}

size_t ff::data_reader::pos() const
{
    return this->data_pos;
}

size_t ff::data_reader::pos(size_t new_pos)
{
    assert(new_pos <= this->size());
    new_pos = std::min(new_pos, this->size());
    this->data_pos = new_pos;
    return new_pos;
}

std::shared_ptr<ff::saved_data_base> ff::data_reader::saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const
{
    auto subdata = this->data->subdata(offset, saved_size);
    return std::make_shared<saved_data_static>(subdata, loaded_size, type);
}

ff::file_reader::file_reader(file_read&& file)
    : file(std::move(file))
{}

ff::file_reader::file_reader(const std::filesystem::path& path)
    : file(path)
{}

ff::file_reader::operator bool() const
{
    return this->file;
}

bool ff::file_reader::operator!() const
{
    return !this->file;
}

size_t ff::file_reader::read(void* data, size_t size)
{
    return file.read(data, size);
}

size_t ff::file_reader::size() const
{
    return file.size();
}

size_t ff::file_reader::pos() const
{
    return file.pos();
}

size_t ff::file_reader::pos(size_t new_pos)
{
    return file.pos(new_pos);
}

std::shared_ptr<ff::saved_data_base> ff::file_reader::saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const
{
    assert(offset + saved_size <= this->size());
    return std::make_shared<saved_data_file>(this->file.path(), offset, saved_size, loaded_size, type);
}

ff::data_writer::data_writer(const std::shared_ptr<std::vector<uint8_t>>& data)
    : data_writer(data, data->size())
{}

ff::data_writer::data_writer(const std::shared_ptr<std::vector<uint8_t>>& data, size_t pos)
    : data(data)
    , data_pos(pos)
{}

size_t ff::data_writer::write(const void* data, size_t size)
{
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
    size_t copy_size = std::min(size, this->size() - this->data_pos);

    if (copy_size)
    {
        std::memcpy(this->data->data() + this->data_pos, bytes, copy_size);
        this->data_pos += copy_size;
        bytes += copy_size;
        size -= copy_size;
    }

    if (size)
    {
        this->data->insert(this->data->cend(), std::initializer_list<uint8_t>(bytes, bytes + size));
        this->data_pos += size;
    }

    return copy_size + size;
}

size_t ff::data_writer::size() const
{
    return this->data->size();
}

size_t ff::data_writer::pos() const
{
    return this->data_pos;
}

size_t ff::data_writer::pos(size_t new_pos)
{
    assert(new_pos <= this->size());
    new_pos = std::min(new_pos, this->size());
    this->data_pos = new_pos;
    return new_pos;
}

std::shared_ptr<ff::saved_data_base> ff::data_writer::saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const
{
    auto subdata = std::make_shared<data_vector>(this->data, offset, saved_size);
    return std::make_shared<saved_data_static>(subdata, loaded_size, type);
}

ff::file_writer::file_writer(file_write&& file)
    : file(std::move(file))
{}

ff::file_writer::file_writer(const std::filesystem::path& path, bool append)
    : file(path, append)
{}

ff::file_writer::operator bool() const
{
    return this->file;
}

bool ff::file_writer::operator!() const
{
    return !this->file;
}

size_t ff::file_writer::write(const void* data, size_t size)
{
    return this->file.write(data, size);
}

size_t ff::file_writer::size() const
{
    return this->file.size();
}

size_t ff::file_writer::pos() const
{
    return this->file.pos();
}

size_t ff::file_writer::pos(size_t new_pos)
{
    return this->file.pos(new_pos);
}

std::shared_ptr<ff::saved_data_base> ff::file_writer::saved_data(size_t offset, size_t saved_size, size_t loaded_size, saved_data_type type) const
{
    assert(offset + saved_size <= this->size());
    return std::make_shared<saved_data_file>(this->file.path(), offset, saved_size, loaded_size, type);
}

size_t ff::stream_copy(writer_base& writer, reader_base& reader, size_t size, size_t chunk_size)
{
    std::vector<uint8_t> buffer;
    size_t copied = 0;

    if (!chunk_size)
    {
        chunk_size = 1024 * 256;
    }

    for (size_t pos = 0; pos < size; pos += chunk_size)
    {
        size_t read_size = std::min(size - pos, chunk_size);
        buffer.resize(read_size);

        read_size = reader.read(buffer.data(), read_size);
        if (!read_size)
        {
            break;
        }

        size_t write_size = writer.write(buffer.data(), read_size);
        copied += write_size;

        if (write_size != read_size)
        {
            assert(false);
            break;
        }
    }

    return copied;
}

Microsoft::WRL::ComPtr<IStream> ff::get_stream(const std::shared_ptr<reader_base>& reader)
{
    return reader ? Microsoft::WRL::ComPtr<IStream>(new ::reader_stream(reader)) : nullptr;
}

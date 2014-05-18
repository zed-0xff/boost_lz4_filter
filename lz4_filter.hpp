#ifndef LZ4_FILTER_HPP_INCLUDED
#define LZ4_FILTER_HPP_INCLUDED

#include <boost/cstdint.hpp> // uint*_t
#include <boost/iostreams/constants.hpp>   // buffer size.
#include <boost/iostreams/detail/config/dyn_link.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
//#include <boost/config/abi_prefix.hpp>

#include <stdexcept>
#include <vector>

// the only macros from <lz4.h> that is necessary in this header
#ifndef LZ4_COMPRESSBOUND
#define LZ4_COMPRESSBOUND(isize)  ((isize) + ((isize)/255) + 16)
#endif

namespace ext { namespace boost { namespace iostreams { namespace lz4 {

const uint32_t legacy_magic = 0x184C2102;

// Do not change this!!
// a) compatibility with lz4c commandline utility will be lost
// b) compatibility with previously compressed data will be lost
const unsigned int legacy_blocksize  = 8*1024*1024; // 8 MB

// no LZ4S format for now, maybe in future..
#if 0
const uint32_t lz4s_magic   = 0x184D2204;

#pragma pack(push,1)
struct lz4s_file_header
    {
        uint32_t magic;
        // descriptor[0]:
        unsigned int dictionary:1;
        unsigned int reserved1:1;
        unsigned int streamChecksumFlag:1;
        unsigned int streamSize:1;
        unsigned int blockChecksumFlag:1;
        unsigned int blockIndependenceFlag:1;
        unsigned int version:2;
        // descriptor[1]:
        unsigned int reserved3:4;
        unsigned int blockSizeId:3;
        unsigned int reserved2:1;
        // descriptor[2]:
        uint8_t checkBits;  // = (xxh32(descriptor,2) >> 8) & 0xff
    };
#pragma pack(pop)
#endif

} // namespace lz4

namespace detail
{

class BOOST_IOSTREAMS_DECL lz4_base
    {
    public:
        typedef char char_type; // required for boost
//        bool good(){ return !m_fail; };

    private:
        bool m_was_header;
        bool m_fail;
        uint32_t m_bytes_needed;
        std::vector <char> m_in_buf, m_out_buf;

        bool decompress_int_buf(const char*&, const char*, char*&, char*, bool);
        bool decompress_ext_buf(const char*&, const char*, char*&, char*, bool);
        int  lz4_decompress(const char*, char*, int);
        void _write_decompressed_buf(char*&dst_begin, char*dst_end);

    protected:
        lz4_base();
        ~lz4_base();
        void init( bool compress );
        void reset(bool compress, bool realloc);
        bool compress_filter(const char*&, const char*, char*&, char*, bool);
        bool decompress_filter(const char*&, const char*, char*&, char*, bool);
    };

//
// Template name: lz4_compressor_impl
// Description: Model of C-Style Filter implementing compression by
//      delegating to the lz4 function deflate.
//
template<typename Alloc = std::allocator<char> >
class lz4_compressor_impl : public lz4_base
    {
    private:
    public:
        lz4_compressor_impl();
        ~lz4_compressor_impl();
        bool filter( const char*& src_begin, const char* src_end,
                     char*& dest_begin, char* dest_end, bool flush );
        void close();
    };

//
// Template name: lz4_compressor
// Description: Model of C-Style Filter implementing decompression by
//      delegating to the lz4 function inflate.
//
template<typename Alloc = std::allocator<char> >
class lz4_decompressor_impl : public lz4_base
    {
    public:
        lz4_decompressor_impl();
        ~lz4_decompressor_impl();
        bool filter( const char*& begin_in, const char* end_in,
                     char*& begin_out, char* end_out, bool flush );
        void close();
    };

} // namespace detail

using namespace ::boost::iostreams;

//
// Template name: lz4_compressor
// Description: Model of InputFilter and OutputFilter implementing
//      compression using lz4.
//
template<typename Alloc = std::allocator<char> >
struct basic_lz4_compressor : symmetric_filter<detail::lz4_compressor_impl<Alloc>, Alloc>
    {
    private:
        typedef detail::lz4_compressor_impl<Alloc>  impl_type;
        typedef symmetric_filter<impl_type, Alloc>  base_type;
    public:
        struct category : dual_use, filter_tag, multichar_tag, closable_tag, optimally_buffered_tag { };
        std::streamsize optimal_buffer_size() const 
            { 
            // boost will try to feed us with blocks of data of this size
            return lz4::legacy_blocksize; 
            }

        typedef typename base_type::char_type               char_type;
//        typedef typename base_type::category                category;
        basic_lz4_compressor();
    };
BOOST_IOSTREAMS_PIPABLE(basic_lz4_compressor, 1)

typedef basic_lz4_compressor<> lz4_compressor;

//
// Template name: lz4_decompressor
// Description: Model of InputFilter and OutputFilter implementing
//      decompression using lz4.
//
template<typename Alloc = std::allocator<char> >
struct basic_lz4_decompressor : symmetric_filter<detail::lz4_decompressor_impl<Alloc>, Alloc>
    {
    private:
        typedef detail::lz4_decompressor_impl<Alloc> impl_type;
        typedef symmetric_filter<impl_type, Alloc>   base_type;
    public:
        struct category : dual_use, filter_tag, multichar_tag, closable_tag, optimally_buffered_tag { };
        std::streamsize optimal_buffer_size() const
            { 
            // filter output buffer will be of this size
            return lz4::legacy_blocksize;
            }

        typedef typename base_type::char_type        char_type;
//        typedef typename base_type::category         category;
        basic_lz4_decompressor( );
    };
BOOST_IOSTREAMS_PIPABLE(basic_lz4_decompressor, 1)

typedef basic_lz4_decompressor<> lz4_decompressor;

//----------------------------------------------------------------------------//

namespace detail
{

//------------------Implementation of lz4_compressor_impl--------------------//

template<typename Alloc>
lz4_compressor_impl<Alloc>::lz4_compressor_impl()
    {
    init(true);
    }

template<typename Alloc>
lz4_compressor_impl<Alloc>::~lz4_compressor_impl()
    {
    reset(true, false);
    }

template<typename Alloc>
bool lz4_compressor_impl<Alloc>::filter
( const char*& src_begin, const char* src_end, char*& dest_begin, char* dest_end, bool flush )
    {
    // call a non-template version of filter, to avoid compiled code duplication
    return compress_filter(src_begin, src_end, dest_begin, dest_end, flush);
    }

template<typename Alloc>
void lz4_compressor_impl<Alloc>::close()
    {
    reset(true, true);
    }

//------------------Implementation of lz4_decompressor_impl------------------//

template<typename Alloc>
lz4_decompressor_impl<Alloc>::lz4_decompressor_impl()
    {
    init(false);
    }

template<typename Alloc>
lz4_decompressor_impl<Alloc>::~lz4_decompressor_impl()
    {
    reset(false, false);
    }

template<typename Alloc>
bool lz4_decompressor_impl<Alloc>::filter
( const char*& src_begin, const char* src_end, char*& dest_begin, char* dest_end, bool flush )
    {
    // call a non-template version of filter, to avoid compiled code duplication
    return decompress_filter(src_begin, src_end, dest_begin, dest_end, flush);
    }

template<typename Alloc>
void lz4_decompressor_impl<Alloc>::close()
    {
    reset(false, true);
    }

} // namespace detail

//------------------Implementation of lz4_decompressor-----------------------//

template<typename Alloc>
basic_lz4_compressor<Alloc>::basic_lz4_compressor() : 
    base_type(4 + sizeof(lz4::legacy_magic) + LZ4_COMPRESSBOUND(lz4::legacy_blocksize))
    {
    }

//------------------Implementation of lz4_decompressor-----------------------//

template<typename Alloc>
basic_lz4_decompressor<Alloc>::basic_lz4_decompressor() : 
    base_type(4 + sizeof(lz4::legacy_magic) + LZ4_COMPRESSBOUND(lz4::legacy_blocksize))
    {
    }

//----------------------------------------------------------------------------//

} // namespace iostreams
} // namespace boost
} // namespace ext

//#include <boost/config/abi_suffix.hpp> // Pops abi_suffix.hpp pragmas.

#endif // #ifndef LZ4_FILTER_HPP_INCLUDED

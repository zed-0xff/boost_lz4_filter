// Define BOOST_IOSTREAMS_SOURCE so that <boost/iostreams/detail/config.hpp>
// knows that we are building the library (possibly exporting code), rather
// than using it (possibly importing code).
// #define BOOST_IOSTREAMS_SOURCE

#include "lz4_filter.hpp"
#include <lz4.h>
#include <iostream>
// do not unpack more data if already have this amount of unpacked data buffered
// highly affects performance!
#define MAX_OUT_BUF (1024 * 1024)

//#define LZ4_FILTER_DEBUG

#define COLOR_RED "\x1b[31m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#ifdef LZ4_FILTER_DEBUG
#define COLOR_GREEN "\x1b[32m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN "\x1b[36m"
#define COLOR_RESET "\x1b[0m"
#define FAIL(msg)                                                   \
  {                                                                 \
    m_fail = true;                                                  \
    printf("%s[!] exception: %s%s\n", COLOR_RED, msg, COLOR_RESET); \
    throw std::runtime_error(msg);                                  \
  }
#else
#define FAIL(msg)                  \
  {                                \
    m_fail = true;                 \
    throw std::runtime_error(msg); \
  }
#endif

namespace ext {
namespace boost {
namespace iostreams {

//------------------Implementation of lz4_base-------------------------------//

namespace detail {

lz4_base::lz4_base() : m_was_header(false), m_fail(false), m_bytes_needed(0) {}

lz4_base::~lz4_base() {
#ifdef LZ4_FILTER_DEBUG
//    printf("[d] %s %p\n", __FUNCTION__, this);
#endif
}

void lz4_base::init(bool /*compress*/) {
  m_in_buf.clear();
  m_out_buf.clear();
  m_was_header = false;
  m_fail = false;
  m_bytes_needed = 0;
}

void lz4_base::reset(bool compress, bool /*realloc*/) { init(compress); }

bool lz4_base::compress_filter(const char*& src_begin, const char* src_end,
                               char*& dst_begin, char* dst_end,
#ifdef LZ4_FILTER_DEBUG
                               bool flush
#else
                               bool  // suppress 'unused parameter' warning
#endif
                               ) {
#ifdef LZ4_FILTER_DEBUG
  printf(
      "\n%s[d] lz4::comp_filter: src_size = %7ld, dst_size = %7ld, flush = %d, "
      "data = %02x %02x %02x%s\n",
      COLOR_MAGENTA, src_end - src_begin, dst_end - dst_begin, flush,
      src_begin[0] & 0xff, src_begin[1] & 0xff, src_begin[2] & 0xff,
      COLOR_RESET);
#endif
  if (!m_was_header) {
    m_was_header = true;
    if ((dst_end - dst_begin) < (int)sizeof(lz4::legacy_magic))
      FAIL("no space to write lz4 header!");
    memcpy(dst_begin, &lz4::legacy_magic, sizeof(lz4::legacy_magic));
    dst_begin += sizeof(lz4::legacy_magic);
#ifdef LZ4_FILTER_DEBUG
    printf("[d] hdr written, sizeof(hdr) = %ld\n", sizeof(lz4::legacy_magic));
#endif
  }
  if (src_begin == src_end) {
    // nothing to compress => EOF
    return false;
  }
  if (dst_end - dst_begin < 4) FAIL("it does not fit! (1)");
  int32_t comp_size = LZ4_compress_limitedOutput(
      src_begin, dst_begin + 4, src_end - src_begin, dst_end - dst_begin - 4);
#ifdef LZ4_FILTER_DEBUG
  printf("[d] comp_size => %7d\n", comp_size);
#endif
  if (comp_size > 0) {
    *(int32_t*)dst_begin = comp_size;  // write compressed chunk size
    dst_begin +=
        comp_size + 4;    // set number of significant bytes in output buffer
    src_begin = src_end;  // mark all input data as consumed
  } else {
    FAIL("it does not fit! (2)");
  }
  // returning FALSE will instruct boost to FLUSH dest buffer to next stream in
  // filter chain
  // thus clearing dest buffer
  return false;
}

bool lz4_base::decompress_filter_input_legacy(const char*& src_begin, const char* src_end,
                         char*& dst_begin, char* dst_end)
{
    unsigned int src_size = src_end - src_begin;
    assert(src_size >= m_bytes_needed);

    m_in_buf.insert(m_in_buf.end(), src_begin, src_begin + m_bytes_needed);
    src_begin += m_bytes_needed;  // consume part of input


    if (m_waitblockstart) 
    {
      // just readed the block size
      m_waitblockstart = false;
      m_bytes_needed = *(int32_t*)&m_in_buf[0];
      if (m_bytes_needed == 0 ||
          m_bytes_needed > LZ4_COMPRESSBOUND(lz4::legacy_blocksize))
        FAIL("invalid lz4 block size!");

      if ((src_end - src_begin) >= m_bytes_needed &&
          (dst_end - dst_begin) >= lz4::legacy_blocksize &&
          m_out_buf.empty()) {
        #ifdef LZ4_FILTER_DEBUG
                    printf("[*] ultra fast path!\n");
        #endif
        // ULTRA-FAST PATH: decompress from src to dst
        uint32_t block_size = m_bytes_needed;
        int raw_size = lz4_decompress(src_begin, dst_begin, block_size);
        dst_begin += raw_size;
        src_begin += block_size;
        m_in_buf.clear();
        m_bytes_needed = 4;  // ready to read next block size
        m_waitblockstart = true;

        // returning TRUE here increases probability to hit ultra-fast path
        // again in
        // next iteration
        return true;
      }
    } 
    else 
    {
      // now have the whole block
      // m_in_buf now contains 4 bytes of block size,
      // followed by block compressed data of that size
      uint32_t block_size = *(uint32_t*)&m_in_buf[0];
      if (block_size != m_in_buf.size() - 4)
        FAIL("block_size != m_in_buf.size() - 4");

      if (m_out_buf.empty() &&
          (dst_end - dst_begin) >= lz4::legacy_blocksize) {
        #ifdef LZ4_FILTER_DEBUG
                    printf("[*] fast path!\n");
        #endif
        // FAST-PATH: decompress directly to dst
        int raw_size = lz4_decompress(&m_in_buf[4], dst_begin, block_size);
        dst_begin += raw_size;
      } else {
        // decompress m_in_buf and APPEND decompressed data to m_out_buf
        std::vector<char>::size_type prev_size = m_out_buf.size();
        m_out_buf.resize(prev_size +
                         lz4::legacy_blocksize);  // pessimistic resize
        int raw_size =
            lz4_decompress(&m_in_buf[4], &m_out_buf[prev_size], block_size);
        m_out_buf.resize(prev_size +
                         raw_size);  // resize to actual data written
      }

      m_in_buf.clear();
      m_waitblockstart = true;
      m_bytes_needed = 4;  // ready to read next block size
    }
    return true;
}
bool lz4_base::decompress_filter_output(char*& dst_begin, char* dst_end)
{
    int dst_size = dst_end - dst_begin;
    if (!dst_size || m_out_buf.empty())
    {
        return false; // nothing done
    }

    // have something to write
    if (dst_size > (int)m_out_buf.size()) 
    {
        // great! can write a whole m_out_buf at once
        std::copy(m_out_buf.begin(), m_out_buf.end(), dst_begin);
        dst_begin += m_out_buf.size();
        m_out_buf.clear();
    } 
    else 
    {
        // not good, need to write only first part of m_out_buf,
        // and then move the remaining data
        std::copy(m_out_buf.begin(), m_out_buf.begin() + dst_size, dst_begin);
        dst_begin = dst_end;
        std::copy(m_out_buf.begin() + dst_size, m_out_buf.end(), m_out_buf.begin());
        m_out_buf.resize(m_out_buf.size() - dst_size);
    }
    return true;
}

bool lz4_base::decompress_filter_header(const char*& src_begin, const char* src_end, bool flush)
{
    if (src_end == src_begin && flush && m_in_buf.empty()) {
      // do not fail if total input data was ZERO bytes (no header, no data,
      // absolutely nothing)
      return false;
    }
    if ((src_end - src_begin) < (int)sizeof(lz4::legacy_magic)) {
      // only will happen when total input data is less than 4 bytes
      // because if it is greater than 4 bytes => boost should try to feed us
      // with blocks of 8 MB each
      FAIL("lz4: too small header!");
    }
    uint32_t magic = *(uint32_t*)src_begin;
    if (magic != lz4::legacy_magic) {
      if (magic != lz4::lz4s_magic) {
        FAIL("not a lz4 legacy or lz4s stream!");
      } else {
        if ((src_end - src_begin) < (int)sizeof(lz4::lz4s_file_header)) {
          FAIL("lz4: too small header for lz4s");
        }
        m_lz4s = true;
      }
    } else
      m_lz4s = false;

#ifdef LZ4_FILTER_DEBUG
    printf("[d] hdr OK!\n");
#endif
    if (m_lz4s) {
      m_lz4s_header = *(lz4::lz4s_file_header*)(src_begin);
      src_begin += sizeof(lz4::lz4s_file_header);
      m_block_uncompressed_max = (1 << (8 + (2 * m_lz4s_header.blockSizeId)));

#ifdef LZ4_FILTER_DEBUG
      std::cerr << "supportedheadersize:" << sizeof(lz4::lz4s_file_header) << std::endl;
      std::cerr << "version:" << m_lz4s_header.version << std::endl;
      std::cerr << "blockSizeId:" << m_lz4s_header.blockSizeId << " ==> bufferSize:" << m_block_uncompressed_max << std::endl;
      std::cerr << "streamChecksumFlag:" << m_lz4s_header.streamChecksumFlag
                << std::endl;
      std::cerr << "dictionary:" << m_lz4s_header.dictionary << std::endl;
      std::cerr << "streamSize:" << m_lz4s_header.streamSize << std::endl;
      std::cerr << "blockChecksumFlag:" << m_lz4s_header.blockChecksumFlag
                << std::endl;
      std::cerr << "blockIndependenceFlag:"
                << m_lz4s_header.blockIndependenceFlag << std::endl;
#endif
      // verifications:
      // version == 1
      // reserved* == 0
      // blockSizeId >= 4 && <= 7
      // stream_size_flag = 0
      // preset_dictionary_flag = 0
      // block_independence_flag = 1
      // block_checksum_flag != 0
      // stream_checksum_flag != 0
      // TBD checksum

      if(m_lz4s_header.streamSize != 0)
      {
        FAIL("LZ4S stream size not supported");
      }
      if(m_lz4s_header.version != 1)
      {
        FAIL("LZ4S version not supported");
      }
      if(m_lz4s_header.blockIndependenceFlag == 0)
      {
        FAIL("LZ4S block dependence not supported");
      }
    } 
    else
    {
      src_begin += sizeof(lz4::legacy_magic);
      m_block_uncompressed_max = lz4::legacy_blocksize;        
    }

    m_was_header = true;
    m_waitblockstart = true;
    m_bytes_needed = 4;  // ready to read 1st block size
  
  return true;
}

// See Also static unsigned long long LZ4IO_decompressLZ4F(dRess_t ress, FILE* srcFile, FILE* dstFile)
// https://github.com/lz4/lz4/blob/dev/programs/lz4io.c
bool lz4_base::decompress_filter_input_lz4s(const char*& src_begin, const char* src_end,
                         char*& dst_begin, char* dst_end)
{
    unsigned int src_size = src_end - src_begin;
    assert(src_size >= m_bytes_needed);

    m_in_buf.insert(m_in_buf.end(), src_begin, src_begin + m_bytes_needed);
    src_begin += m_bytes_needed;  // consume part of input

    if(m_waitblockstart)
    {
        m_waitblockstart = false;
        uint32_t block_size = *(uint32_t*)&m_in_buf[0];
        m_block_uncompressed = (block_size & 0x80000000) != 0;
        m_block_size = block_size & 0x7FFFFFFF;
        if(m_block_size == 0)
        {
            if(m_lz4s_header.streamChecksumFlag && src_end-src_begin >= 4)
            {
                src_begin += 4;
            }
            m_bytes_needed = 0;
            return true;
        }
        else if(m_block_size > m_block_uncompressed_max)
        {
            FAIL("ERROR IN SIZE")
        }
        else
        {
            m_bytes_needed = m_block_size + (m_lz4s_header.blockChecksumFlag ? 4 : 0);
         }

         // BUFFER contains only HEADER

      if ((src_end - src_begin) >= m_bytes_needed &&
          (dst_end - dst_begin) >= m_block_uncompressed_max &&
          m_out_buf.empty()) {
        #ifdef LZ4_FILTER_DEBUG
                    printf("[*] ultra fast path!\n");
        #endif
        // ULTRA-FAST PATH: decompress from src to dst
        uint32_t block_size = m_block_size;
        int raw_size = lz4_decompress(src_begin, dst_begin, block_size,m_block_uncompressed_max);
        dst_begin += raw_size;
        src_begin += block_size;
        m_in_buf.clear();
        m_bytes_needed = 4;  // ready to read next block size
        m_waitblockstart = true;
        return true;
        }
    }     
    else
    {
        // decompress m_in_buf and APPEND decompressed data to m_out_buf
        std::vector<char>::size_type prev_size = m_out_buf.size();


        if(m_block_uncompressed)
        {
              if (m_out_buf.empty() &&
                          (dst_end - dst_begin) >= m_block_size) {
                        #ifdef LZ4_FILTER_DEBUG
                                    printf("[*] fast path!\n");
                        #endif
                        std::copy_n(m_in_buf.begin()+4,m_block_size, dst_begin);
                        dst_begin += m_block_size;
                } 
                else 
                {
                    m_out_buf.resize(prev_size + m_block_size);
                    std::copy_n(m_in_buf.begin()+4,m_block_size,m_out_buf.begin()+prev_size);
                }
        }
        else
        {

              if (m_out_buf.empty() &&
                  (dst_end - dst_begin) >= m_block_uncompressed_max) {
                    // FAST-PATH: decompress directly to dst
                    int raw_size = lz4_decompress(&m_in_buf[4], dst_begin, m_block_size);
                    dst_begin += raw_size;
              } 
              else 
              {

                m_out_buf.resize(prev_size + m_block_uncompressed_max);

                int raw_size = lz4_decompress(&m_in_buf[4], &m_out_buf[prev_size], m_block_size, m_out_buf.size()-prev_size);
                m_out_buf.resize(prev_size + raw_size);  // resize to actual data written
            }
        }

        // next block => skip automatically checksum if present
        m_in_buf.clear();
        m_waitblockstart = true;
        m_bytes_needed = 4;  // ready to read next block size
    }; 
    return true;
}

bool lz4_base::decompress_filter(const char*& src_begin, const char* src_end,
                                 char*& dst_begin, char* dst_end, bool flush) {
#ifdef LZ4_FILTER_DEBUG
  printf(
      "\n%s[d] lz4::decomp_filter enter: src_sz = %7ld, dst_sz = %7ld, flush = %d, "
      "in buffer = %ld, out buffer = %ld wait = %s needed = %d dataprefix = %02x %02x %02x %02x%s\n",
      COLOR_CYAN, src_end - src_begin, dst_end - dst_begin, flush,
      m_in_buf.size(), m_out_buf.size(), 
      !m_was_header ? "header": m_waitblockstart ? "blocksize" : "blockdata",m_bytes_needed,src_begin[0] & 0xff,
      src_begin[1] & 0xff, src_begin[2] & 0xff, src_begin[3] & 0xff,
      COLOR_RESET);
#endif

  if (m_fail) 
  {
    // decompression failed, do not try to process anything else
    return false;
  }

  if(!m_was_header)
  {
      if(!decompress_filter_header(src_begin,src_end,flush))
      {
            return false;
        }
  }

  // consume output buffer if ANY
  decompress_filter_output(dst_begin,dst_end);

  // process input as long as buffer is not full
    while (src_begin < src_end && m_out_buf.size() < MAX_OUT_BUF) 
    {
        const unsigned int src_size = src_end - src_begin;
        if (src_size >= m_bytes_needed) 
        {
            if(m_lz4s)
            {
                if(!decompress_filter_input_lz4s(src_begin,src_end,dst_begin,dst_end))
                    break;
            }
            else
            {
                if(!decompress_filter_input_legacy(src_begin,src_end,dst_begin,dst_end))
                    break;
            }
        }
        else
        {
            m_in_buf.insert( m_in_buf.end(), src_begin, src_end );
            src_begin = src_end;
            m_bytes_needed -= src_size;            
        }
        decompress_filter_output(dst_begin,dst_end);
    }
    // another consume output
    decompress_filter_output(dst_begin,dst_end);

#ifdef LZ4_FILTER_DEBUG
    printf("[d] lz4::decomp_filter exit m_in_buf: %ld, m_out_buf: %ld, m_bytes_needed = %d wait = %s input left = %d\n",
           m_in_buf.size(), m_out_buf.size(), m_bytes_needed,!m_was_header ? "header": m_waitblockstart ? "blocksize" : "blockdata",src_end - src_begin);
#endif
  
  if (flush) 
  {
    if (!m_in_buf.empty() && src_end-src_begin > 0) 
    {
        FAIL("lz4: unexpected EOF");
    }
    return !m_out_buf.empty();
  } 
  else 
  {
    return true;
  }
}

int lz4_base::lz4_decompress(const char* src_begin, char* dst_begin,
                             int comp_chunk_size, int uc) {
  int raw_size = LZ4_decompress_safe(src_begin, dst_begin, comp_chunk_size,
                                     uc);
#ifdef LZ4_FILTER_DEBUG
  printf("[d] decompressed size = %d\n", raw_size);
#endif
  if (raw_size <= 0) {
    FAIL("lz4: decoded_size <= 0");
  }
  return raw_size;
}

}  // namespace detail

//----------------------------------------------------------------------------//

}  // namespace iostreams
}  // namespace boost
}  // namespace ext

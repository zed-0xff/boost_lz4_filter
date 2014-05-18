#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>
#include "../lz4_filter.hpp"

namespace bio = boost::iostreams;
namespace ext { namespace bio = ext::boost::iostreams; }

// reference raw (uncompressed) data (100x repeat of "1234567890")
const char* ref_raw_data = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";

// reference compressed data
const uint8_t ref_comp_data[] = {
    // header magic
    0x02, 0x21, 0x4c, 0x18,
    // chunk 1 size
    0x17, 0x00, 0x00, 0x00,  
    // chunk 1 data
    0xaf, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 
    0x38, 0x39, 0x30, 0x0a, 0x00, 0xff, 0xff, 0xff, 
    0xc9, 0x50, 0x36, 0x37, 0x38, 0x39, 0x30
};

TEST(lz4_decompress, streambuf_reference_data) {
    std::stringstream src(std::string((const char*)ref_comp_data, sizeof(ref_comp_data)));
    bio::filtering_streambuf<bio::input> in;
    in.push( ext::bio::lz4_decompressor() );
    in.push( src );

    std::istream is(&in);
    char out_buf[0x1000];
    is.read( out_buf, sizeof(out_buf) );
    ASSERT_EQ( std::string(out_buf), std::string(ref_raw_data) );
}

TEST(lz4_compress, streambuf_reference_data) {
    std::stringstream src(ref_raw_data);
    bio::filtering_streambuf<bio::input> in;
    in.push( ext::bio::lz4_compressor() );
    in.push( src );

    std::istream is(&in);
    char out_buf[0x1000];
    is.read( out_buf, sizeof(out_buf) );
    ASSERT_EQ( std::string(out_buf, is.gcount()), std::string((const char*)ref_comp_data, sizeof(ref_comp_data)) );
}

TEST(lz4_compress, reference_data) {
    std::stringbuf buf;
    std::ostream out( &buf );
    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_compressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );

        bifo << ref_raw_data;
    }
    ASSERT_EQ( buf.str(), std::string((const char*)ref_comp_data, sizeof(ref_comp_data)) );
}


TEST(lz4_decompress, reference_data_string) {
    std::stringbuf buf;
    std::ostream out( &buf );
    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_decompressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );

        bifo << std::string((const char*)ref_comp_data, sizeof(ref_comp_data));
    }
    ASSERT_EQ( buf.str(), ref_raw_data );
}

TEST(lz4_decompress, reference_data_write) {
    std::stringbuf buf;
    std::ostream out( &buf );
    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_decompressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );

        bifo.write( (char*)ref_comp_data, sizeof(ref_comp_data) );
    }
    ASSERT_EQ( buf.str(), ref_raw_data );
}

TEST(lz4_decompress, reference_data_boost_copy_ostream) {
    std::stringbuf obuf;
    std::ostream out( &obuf );
    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_decompressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );

        std::stringbuf ibuf( std::string((const char*)ref_comp_data, sizeof(ref_comp_data)) );
        std::istream in( &ibuf );
        boost::iostreams::copy(in, bifo);
    }
    ASSERT_EQ( obuf.str(), ref_raw_data );
}

TEST(lz4_decompress, reference_data_boost_copy_istream) {
    std::stringbuf ibuf(std::string((const char*)ref_comp_data, sizeof(ref_comp_data)));
    std::stringbuf obuf;
    std::ostream out( &obuf );
    {
        bio::filtering_istream bifi;
        std::istream in( &ibuf );
        bifi.push( ext::bio::lz4_decompressor() );
        bifi.push( in );
        bifi.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );
        std::ostream out( &obuf );
        boost::iostreams::copy(bifi, out);
    }
    ASSERT_EQ( obuf.str(), ref_raw_data );
}

TEST(lz4_compress, compressible_data) {
    std::stringbuf buf;
    std::ostream out( &buf );
    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_compressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );
        for( int i=0; i<500000; i++ ){
            bifo << "123456789012345678901234567890";
        }
    }
    ASSERT_EQ( 58874, out.tellp());
}

// lz4_filter block size = 8MB, so we take 10MB of non-compressible data
#define RANDOM_DATA_SIZE (10*1024*1024)

TEST(lz4_compress, random_data) {
    std::stringbuf buf;
    std::ostream out( &buf );

    std::vector <char> random_data( RANDOM_DATA_SIZE );
    std::ifstream ifurandom( "/dev/urandom" );
    ifurandom.read( &random_data[0], RANDOM_DATA_SIZE );

    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_compressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );

        for( size_t total_size=0; total_size < (100*1024*1024); total_size += RANDOM_DATA_SIZE ){
            bifo.write(&random_data[0], RANDOM_DATA_SIZE);
        }
    }

    ASSERT_EQ( 105268882, out.tellp() );
}

TEST(lz4_comp_decomp, random_data) {
    std::stringbuf buf;
    std::ostream out( &buf );

    std::vector <char> random_data( RANDOM_DATA_SIZE );
    std::ifstream ifurandom( "/dev/urandom" );
    ifurandom.read( &random_data[0], RANDOM_DATA_SIZE );

    size_t total_size=0;
    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_compressor() );
        bifo.push( ext::bio::lz4_decompressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );

        for(; total_size < (100*1024*1024); total_size += RANDOM_DATA_SIZE ){
            bifo.write(&random_data[0], RANDOM_DATA_SIZE);
        }
    }

    ASSERT_EQ( total_size, out.tellp() );
}

TEST(lz4_comp_decomp, ref_data) {
    std::stringbuf buf;
    std::ostream out( &buf );

    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_compressor() );
        bifo.push( ext::bio::lz4_decompressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );

        for( int i=0; i<100; i++){
            bifo << "1234567890";
        }
    }

    ASSERT_EQ( ref_raw_data, buf.str() );
}

TEST(lz4_comp_decomp, ref_data_10000x_in_single_chain) {
    std::stringbuf buf;
    std::ostream out( &buf );
    std::string check_data;

    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_compressor() );
        bifo.push( ext::bio::lz4_decompressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );

        for( int j=0; j<10000; j++){
            for( int i=0; i<100; i++){
                bifo << "1234567890";
                check_data += "1234567890";
            }
        }
    }

    ASSERT_EQ( check_data, buf.str() );
}

TEST(lz4_comp_decomp, ref_data_10000x_in_separate_chains) {
    std::stringbuf buf;
    std::ostream out( &buf );
    std::string check_data;

    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_compressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );

        for( int j=0; j<10000; j++){
            for( int i=0; i<100; i++){
                bifo << "1234567890";
                check_data += "1234567890";
            }
        }
    }

    std::stringbuf buf2;
    std::ostream out2( &buf2 );
    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_decompressor() );
        bifo.push( out2 );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );
        //bifo << buf.str();
        std::istream in(&buf);
        boost::iostreams::copy(in, bifo);
    }

    ASSERT_EQ( check_data, buf2.str() );
}

void test_comp_decomp(int size, const char* substr = "1"){
    std::string s;
    for(int i=0; i<size; i++) s += substr;
    ASSERT_EQ( s.size(), size*strlen(substr));

    std::stringbuf buf;
    {
        std::ostream out( &buf );
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_compressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );
        bifo << s;
    }

    std::string s1;
    {
        bio::filtering_istream bifi;
        std::istream in( &buf );
        bifi.push( ext::bio::lz4_decompressor() );
        bifi.push( in );
        bifi.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );
        std::stringbuf buf1;
        std::ostream out( &buf1 );
        boost::iostreams::copy(bifi, out);
        s1 = buf1.str();
    }

    ASSERT_EQ( s, s1 );
}

TEST(lz4_comp_decomp, misc_data) {
    for( int i=0; i<2000; i++){
        test_comp_decomp(i);
        test_comp_decomp(i, "foobarbazXXX");
        test_comp_decomp(i, "asfalweufcn298fh298 3h293 892h asofa f -2094 8 1=498r =09f a=s09fu  0294u -q04");
    }
}

TEST(lz4_decompress, bad_hdr) {
    const uint32_t data[] = {0xdeadf00d};
    
    std::stringbuf buf;
    {
        bio::filtering_ostream bifo;
        std::ostream out( &buf );
        bifo.push( ext::bio::lz4_decompressor() );
        bifo.push( out );
        bifo.exceptions( std::ofstream::eofbit | std::ofstream::failbit | std::ofstream::badbit );
        boost::iostreams::write(bifo, (char*)data, sizeof(data));
        ASSERT_THROW( bifo.flush(), std::exception );
    }
}

TEST(lz4_decompress, negative_chunk_size) {
    const uint8_t data[] = {
        // header
        0x02, 0x21, 0x4c, 0x18, 
        // chunk1 size
        0x17, 0x00, 0x00, 0x00, 
        // chunk1 data:
        0xaf, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 
        0x38, 0x39, 0x30, 0x0a, 0x00, 0xff, 0xff, 0xff, 
        0xc9, 0x50, 0x36, 0x37, 0x38, 0x39, 0x30,

         // chunk2 size (BAD - negative)
        0x17, 0x00, 0x00, 0xFF, 
        // chunk2 data:
        0xaf, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 
        0x38, 0x39, 0x30, 0x0a, 0x00, 0xff, 0xff, 0xff, 
        0xc9, 0x50, 0x36, 0x37, 0x38, 0x39, 0x30,
    };

    std::stringbuf buf;
    std::ostream out( &buf );
    {
        bio::filtering_ostream bifo;
        bifo.push( ext::bio::lz4_decompressor() );
        bifo.push( out );
        bifo.exceptions( std::ifstream::failbit | std::ifstream::badbit );

        bifo << std::string((const char*)data, sizeof(data));
        ASSERT_THROW( bifo.flush(), std::exception );
    }
}

void test_premature_eof( int size ){
    std::stringbuf buf(std::string((const char*)ref_comp_data, size));
    {
        bio::filtering_istream bifi;
        std::istream in( &buf );
        bifi.push( ext::bio::lz4_decompressor() );
        bifi.push( in );
        bifi.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );
        std::ofstream out;
        ASSERT_THROW( boost::iostreams::copy(bifi, out), std::runtime_error ) << size;
    }
}

TEST(lz4_decompress, premature_eof) {
    for( unsigned int i=1; i<sizeof(ref_comp_data); i++ ){
        if( i==4 ){
            // valid data (header only)
            continue;
        }
        test_premature_eof(i);
    }
}

TEST(lz4_decompress, onebyte_bigger_chunk_size) {
    const uint8_t data[] = {
        // header
        0x02, 0x21, 0x4c, 0x18, 
        // chunk1 size
        0x18, 0x00, 0x00, 0x00, // <-- 0x18 instead of 0x17
        // chunk1 data:
        0xaf, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 
        0x38, 0x39, 0x30, 0x0a, 0x00, 0xff, 0xff, 0xff, 
        0xc9, 0x50, 0x36, 0x37, 0x38, 0x39, 0x30
    };

    std::stringbuf buf(std::string((const char*)data, sizeof(data)));
    {
        bio::filtering_istream bifi;
        std::istream in( &buf );
        bifi.push( ext::bio::lz4_decompressor() );
        bifi.push( in );
        bifi.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );
        std::ofstream out;
        ASSERT_THROW( boost::iostreams::copy(bifi, out), std::runtime_error );
    }
}

void test_decompress_bad_data(int size, const char* substr = "1"){
    std::string s;
    for(int i=0; i<size; i++) s += substr;
    ASSERT_EQ( s.size(), size*strlen(substr));

    std::stringbuf buf(s);
    {
        bio::filtering_istream bifi;
        std::istream in( &buf );
        bifi.push( ext::bio::lz4_decompressor() );
        bifi.push( in );
        std::ofstream out;
        ASSERT_THROW( boost::iostreams::copy(bifi, out), std::runtime_error );
    }
}

// generates a blocks of data from 0 to 1000 bytes, and tries to decompress them
TEST(lz4_decompress, bad_data) {
    for(int i=1; i<=1000; i++){
        test_decompress_bad_data(i);
    }
}

TEST(lz4_decompress, zero_length_data) {
    std::stringbuf buf;
    std::ostream out( &buf );
    {
        bio::filtering_istream bifi;
        std::stringbuf buf;
        std::istream in( &buf );
        bifi.push( ext::bio::lz4_decompressor() );
        bifi.push( in );
        boost::iostreams::copy(bifi, out);
    }
    ASSERT_EQ( buf.str().size(), 0 );
}

// no LZ4S format for now
#if 0
TEST(lz4s, header_size) {
    ASSERT_EQ( sizeof(ext::bio::lz4::lz4s_file_header), 7);
}

TEST(lz4s, header_decode) {
    std::ifstream in("gtests/data/reports-for-planning.lz4s");
    ext::bio::lz4::lz4s_file_header hdr;
    in.read( (char*)&hdr, sizeof(hdr) );

    printf("hdr.magic                 = %x\n", hdr.magic);
    printf("hdr.dictionary            = %x\n", hdr.dictionary);
    printf("hdr.reserved1             = %x\n", hdr.reserved1);
    printf("hdr.streamChecksumFlag    = %x\n", hdr.streamChecksumFlag);
    printf("hdr.streamSize            = %x\n", hdr.streamSize);
    printf("hdr.blockChecksumFlag     = %x\n", hdr.blockChecksumFlag);
    printf("hdr.blockIndependenceFlag = %x\n", hdr.blockIndependenceFlag);
    printf("hdr.version               = %x\n", hdr.version);
    printf("hdr.reserved3             = %x\n", hdr.reserved3);
    printf("hdr.blockSizeId           = %x\n", hdr.blockSizeId);
    printf("hdr.reserved2             = %x\n", hdr.reserved2);
    printf("hdr.checkBits             = %x\n", hdr.checkBits);
}
#endif

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}


#include <iostream>
#include <fstream>
#include <random>
#include <thread>
#include <iomanip>
#include <cstring>
/* BOOST */
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include "lz4_filter.hpp"

#define COLOR_GREEN "\x1b[32m"
#define COLOR_RED "\x1b[31m"
    template<typename T>
    using deleted_unique_ptr = std::unique_ptr<T,std::function<void(T*)>>;

int main(int argc, char const *argv[])
{   
    if(argc < 2)
    {
        return -1;
    }

    std::string filename = argv[1];

    std::ifstream realfilelz4,realfilegz;
    boost::iostreams::filtering_streambuf<boost::iostreams::input> biolz4;
    boost::iostreams::filtering_streambuf<boost::iostreams::input> biogz;
    deleted_unique_ptr<std::istream> iolz4,iogz; // DeleterOwned

    realfilelz4.open(filename+".lz4",std::ifstream::in | std::ifstream::binary);

    if(!realfilelz4.is_open())
    {
        std::cerr << "cannot open file with " << filename<<".lz4" << std::endl;
        return 0;
    }
    realfilegz.open(filename+".gz",std::ifstream::in | std::ifstream::binary);
    if(!realfilelz4.is_open())
    {
        std::cerr << "cannot open file with +.gz" << std::endl;
        return 0;
    }

    biogz.push(boost::iostreams::gzip_decompressor());
    biogz.push(realfilegz);
    iogz = deleted_unique_ptr<std::istream>(new std::istream(&biogz), [](std::istream*p) { delete p; }); // DeleterOwned(true));   

    biolz4.push(ext::boost::iostreams::lz4_decompressor());
    biolz4.push(realfilelz4);
    iolz4 = deleted_unique_ptr<std::istream>(new std::istream(&biolz4), [](std::istream*p) { delete p; }); // DeleterOwned(true));   

    static char buf1[128*1024],buf2[128*1024];
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, sizeof(buf1));

    size_t offset = 0;
    while(true)
    {   
        int q = dis(gen);
        iogz->read(buf1,q);
        iolz4->read(buf2,q);
        int ngz = iogz->gcount(),nlz4 = iolz4->gcount();
        if(ngz != nlz4)
        {
            std::cerr << COLOR_RED <<  "@" << offset << " different read gz:" << ngz << " vs lz4:" << nlz4 << std::endl;
            break;
        }
        else if(!ngz)
            break;
        else
        {
            if(memcmp(buf1,buf2,ngz) != 0)
            {
                std::cerr << COLOR_RED << "@" << offset << " different content sized " << ngz << std::endl;
                break;
            }
            else
            {
                offset += ngz;
                //std::cerr <<COLOR_GREEN << "@" << offset << " " << ngz << std::endl;
            }
        }
        if(offset % 128*1024*1024 == 0)
            std::cerr << COLOR_GREEN << "@" << offset << std::endl;
    }
    std::cerr << "processed " << offset << " bytes " << std::endl;
    return 0;
}
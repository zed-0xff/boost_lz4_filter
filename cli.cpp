#include "lz4_filter.hpp"
#include <fstream>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/copy.hpp>

using namespace std;

namespace bio = boost::iostreams;
namespace ext { namespace bio = ext::boost::iostreams; }

int main(int argc, char** argv){
	if( 2 != argc ){
		cout << "USAGE: " << endl
		     << "\t" << argv[0] << " -c   - compress STDIN to STDOUT" << endl
		     << "\t" << argv[0] << " -d   - decompress STDIN to STDOUT" << endl;
		return 1;
	}

	if( 2 != strlen(argv[1])){
		cerr << "error: invalid argument length!" << endl;
		return 2;
	}

	if( '-' != argv[1][0] ){
		cerr << "error: invalid argument!" << endl;
		return 3;
	}

        bio::filtering_ostream bifo;

	switch(argv[1][1]){
		case 'c':
			bifo.push( ext::bio::lz4_compressor() );
			break;
		case 'd':
			bifo.push( ext::bio::lz4_decompressor() );
			break;
		default:
			cerr << "error: invalid argument!" << endl;
			return 3;
	}

        bifo.push( cout );
        bifo.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );
        boost::iostreams::copy(cin, bifo);

	return 0;
}

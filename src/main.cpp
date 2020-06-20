/*
    This file is part of sconv.

    sconv is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    sconv is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with sconv.  If not, see <https://www.gnu.org/licenses/>.
 * */

#include <iostream>
#include <cstdio>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iconv.h>
#include <getopt.h>

namespace {
	const char*	VERSION = "0.0.1";

	// settings/options management
	std::string	outfile;

	void print_help(const char *prog, const char *version) {
		std::cerr <<	"Usage: " << prog << " [options] (input file)\nExecutes sconv " << version << "\n\n"
				"Converts an input file (or STDIN when not specified) from a given format\n"
				"to another file (or STDOUt when not set)\n\n"
				"-o, --output-file f Specifies output file (f) to be written; when not set, STDOUT\n"
				"                    is used as output\n"
				"    --help          prints this help and exit\n\n"
		<< std::flush;
	}

	int parse_args(int argc, char *argv[], const char *prog, const char *version) {
		int			c;
		static struct option	long_options[] = {
			{"output-file",		required_argument, 0,	'o'},
			{"help",		no_argument,	   0,	0},
			{0, 0, 0, 0}
		};

		while (1) {
			// getopt_long stores the option index here
			int		option_index = 0;

			if(-1 == (c = getopt_long(argc, argv, "o:", long_options, &option_index)))
				break;

			switch (c) {
			case 0: {
				// If this option set a flag, do nothing else now
				if (long_options[option_index].flag != 0)
					break;
				if(!std::strcmp("help", long_options[option_index].name)) {
					print_help(prog, version);
					std::exit(0);
				}
			} break;

			case 'o': {
				outfile = optarg;
			} break;

			case '?':
			break;

			default:
				throw std::runtime_error((std::string("Invalid option '") + (char)c + "'").c_str());
			}
		}
		return optind;
	}

	std::string get_basedir(const char *fname) {
		const auto p = std::strrchr(fname, '/');
		if(!p)
			return "";
		return std::string(fname, p+1);
	}
}

int main(const int argc, char *argv[]) {
	try {
		// convert stdin from utf8 to wchar_t
		const size_t	buf_sz = 4096;
		uint8_t		buf[buf_sz];
		ssize_t		rv = -1,
				total_cnv = 0;
		std::string	tmpfile;

		// parse args first
		const auto optind = parse_args(argc, argv, argv[0], VERSION);

		// prepare input and output files
		int	in_fd = STDIN_FILENO,
			out_fd = STDOUT_FILENO;
		if(optind < argc) {
			in_fd = open(argv[optind], O_RDONLY);
			if(-1 == in_fd)
				throw std::runtime_error((std::string("Can't open file '") + argv[optind] + "' as input").c_str());
		}
		if(!outfile.empty()) {
			// create a temporary file
			// to write and then swap at the end
			char	buf[256];
			std::snprintf(buf, 255, "%ssconv-XXXXXX", get_basedir(outfile.c_str()).c_str());
			out_fd = mkstemp(buf);
			if(-1 == out_fd)
				return false;
			tmpfile = buf;
		}

		while((rv = read(in_fd, buf, buf_sz-1)) > 0) {
			wchar_t		out[rv];
			auto		conv = iconv_open("WCHAR_T", "UTF-8");
			char		*pIn = (char*)buf,
					*pOut = (char*)&out[0];
			size_t		sIn = rv,
					sOut = rv*sizeof(wchar_t);
			if(((void*)-1) == conv)
				throw std::runtime_error("This system can't convert from UTF-8 to WCHAR_T");
			iconv(conv, &pIn, &sIn, &pOut, &sOut);
			iconv_close(conv);
			const ssize_t	to_write = rv*sizeof(wchar_t) - sOut;
			if(to_write != write(out_fd, out, to_write))
				throw std::runtime_error("Couldn't write the required bytes to stdout");
			total_cnv += to_write;
		}
		// when we're done and we have an output file
		// atomically rename it
		if(!outfile.empty()) {
			close(out_fd);
			if(chmod(tmpfile.c_str(), S_IRWXU|S_IRGRP|S_IROTH))
				throw std::runtime_error("Can't change permissions of output file");
			if(std::rename(tmpfile.c_str(), outfile.c_str()))
				throw std::runtime_error("Can't swap temp file to output file");
		}
		std::cerr << "Written: " <<  total_cnv << " bytes" << std::endl;
	} catch(const std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	} catch(...) {
		std::cerr << "Unknown exception" << std::endl;
	}
}

